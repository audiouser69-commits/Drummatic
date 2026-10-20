#pragma once
#include "Common.h"
#include <memory>

namespace mp
{
// Motori sampler. Aggiungere i nuovi SOLO in fondo.
enum SamplerEngineId { SmpClassic, SmpPhaseWarp, SmpCloner, SmpGranular, kNumSamplerEngines };
struct SamplerInfo { const char* name; const char* k[4]; };
inline const SamplerInfo& samplerInfo (int id)
{
    static const SamplerInfo i[] = {
        { "Classic",    { "FINE", "TILT", "COMP", "GAIN" } },
        { "Phase Warp", { "MIRROR", "SHAPE", "SPLIT", "SWING" } },
        { "Cloner",     { "FINE", "TILT", "VARIATION", "DAMP" } },
        { "Granular",   { "SIZE", "RATE", "SPEED", "BLUR" } },
    };
    return i[std::min (std::max (id, 0), (int) kNumSamplerEngines - 1)];
}

// Dati del campione condivisi (immutabili dopo il caricamento)
struct SampleData
{
    std::vector<float> l, r;
    float sourceRate = 48000;
    int length() const { return (int) l.size(); }
};

struct SamplerVoice
{
    float sr = 48000;
    double pos = 0; bool playing = false; bool reverse = false;
    int engine = SmpClassic;
    OnePole tiltLp, tiltHp; float compEnv = 0;
    // Phase Warp
    double t = 0;
    // Cloner: banco di filtri con guadagni/decadimenti per colpo
    static constexpr int NB = 8;
    Svf bandL[NB], bandR[NB]; float bandGain[NB] {}, bandEnv[NB] {}, bandDecay[NB] {}; float cloneDetune = 0;
    // Granular
    struct Grain { double pos = 0, inc = 0; int age = 0, len = 0; bool active = false; };
    static constexpr int NG = 32;
    Grain grains[NG]; double playhead = 0; float grainTimer = 0; int granElapsed = 0;
    Rng rng { 0x5A11 };

    void prepare (float sampleRate) { sr = sampleRate; }

    void trigger (const SampleData* s, float start01, float end01, int eng, const float* p)
    {
        engine = eng;
        if (s == nullptr || s->length() < 2) { playing = false; return; }
        const int n = s->length();
        const double a = (double) clamp01 (start01) * (n - 1), b = (double) clamp01 (end01) * (n - 1);
        reverse = b < a;
        pos = a; t = 0;
        playing = std::abs (b - a) >= 1.0;
        if (engine == SmpCloner)
        {
            const float var = p[2] * 2.0f;          // 0.5 = tipico
            for (int i = 0; i < NB; ++i)
            {
                bandGain[i] = std::max (0.0f, 1.0f + rng.bi() * 0.35f * var * var);
                bandEnv[i] = 1.0f;
                bandDecay[i] = 1.0f;
                bandL[i].reset(); bandR[i].reset();
            }
            cloneDetune = rng.bi() * 0.08f * var;
        }
        if (engine == SmpGranular)
        {
            for (auto& g : grains) g.active = false;
            playhead = 0; grainTimer = 1.0f; granElapsed = 0;
        }
    }

    // pitchSt = semitoni totali; p[0..3] = manopole del motore (0..1, 0.5 neutro)
    void process (const SampleData* s, float start01, float end01, float pitchSt, const float* p, float& outL, float& outR)
    {
        outL = outR = 0;
        if (! playing || s == nullptr || s->length() < 2) { playing = false; return; }
        switch (engine)
        {
            case SmpPhaseWarp: processWarp (s, start01, end01, pitchSt, p, outL, outR); return;
            case SmpGranular:  processGranular (s, start01, end01, pitchSt, p, outL, outR); return;
            case SmpCloner:    processCloner (s, start01, end01, pitchSt, p, outL, outR); return;
            default:           processClassic (s, start01, end01, pitchSt, p, outL, outR); return;
        }
    }

private:
    static void readAt (const SampleData* s, double pos, float& L, float& R)
    {
        const int n = s->length();
        if (pos < 0 || pos > n - 1) { L = R = 0; return; }
        const int i0 = (int) pos; const float fr = (float) (pos - i0);
        const int i1 = std::min (n - 1, i0 + 1);
        L = s->l[(size_t) i0] + (s->l[(size_t) i1] - s->l[(size_t) i0]) * fr;
        R = s->r.empty() ? L : s->r[(size_t) i0] + (s->r[(size_t) i1] - s->r[(size_t) i0]) * fr;
    }

    // avanzamento lineare comune (Classic / Cloner); restituisce false a fine regione
    bool linearRead (const SampleData* s, float start01, float end01, double step, float& L, float& R)
    {
        const int n = s->length();
        const double a = (double) clamp01 (start01) * (n - 1), b = (double) clamp01 (end01) * (n - 1);
        const double lo = std::min (a, b), hi = std::max (a, b);
        if (pos < lo - 1 || pos > hi + 1) { playing = false; return false; }
        readAt (s, pos, L, R);
        pos += reverse ? -step : step;
        if (reverse ? pos <= b : pos >= b) playing = false;
        return true;
    }

    void applyTilt (float& L, float& R, float tilt)
    {
        if (tilt < 0.49f)
        {
            tiltLp.setCutoff (20000.0f * std::pow (0.004f, (0.5f - tilt) * 2.0f), sr);
            const float m = tiltLp.lp ((L + R) * 0.5f); const float side = (L - R) * 0.5f * (tilt * 2.0f);
            L = m + side; R = m - side;
        }
        else if (tilt > 0.51f)
        {
            tiltHp.setCutoff (20.0f * std::pow (400.0f, (tilt - 0.5f) * 2.0f), sr);
            const float d = tiltHp.lp ((L + R) * 0.5f);
            L -= d; R -= d;
        }
    }

    void processClassic (const SampleData* s, float st, float en, float pitchSt, const float* p, float& outL, float& outR)
    {
        float L, R;
        const double step = (double) semis (pitchSt + (p[0] - 0.5f) * 2.0f) * s->sourceRate / sr;
        if (! linearRead (s, st, en, step, L, R)) return;
        applyTilt (L, R, p[1]);
        // COMP: >0.5 compressione, <0.5 espansione
        const float lvl = std::max (std::abs (L), std::abs (R));
        compEnv = lvl > compEnv ? lvl : compEnv * 0.9995f;
        const float c = (p[2] - 0.5f) * 2.0f;
        float g = 1.0f;
        if (c > 0) g = std::pow (std::max (compEnv, 1.0e-3f), -c * 0.7f) * (1.0f - c * 0.3f);
        else if (c < 0) g = std::pow (std::max (compEnv, 1.0e-3f), -c * 1.2f);
        g = clampf (g, 0.0f, 16.0f);
        L *= g; R *= g;
        const float gain = std::pow (10.0f, (p[3] - 0.5f) * 2.0f * 1.2f);
        if (p[3] > 0.5f) { L = fastTanh (L * gain); R = fastTanh (R * gain); }
        else { L *= gain; R *= gain; }
        outL = L; outR = R;
    }

    // PHASE WARP: piega la linea temporale 0→1 della riproduzione
    void processWarp (const SampleData* s, float st, float en, float pitchSt, const float* p, float& outL, float& outR)
    {
        const int n = s->length();
        const double a = (double) clamp01 (st) * (n - 1), b = (double) clamp01 (en) * (n - 1);
        const double regionLen = std::max (1.0, std::abs (b - a));
        const double step = (double) semis (pitchSt) * s->sourceRate / sr;
        double u = t;
        // SHAPE: accelera (>0.5) o rallenta (<0.5) nel corso del campione
        u = std::pow (u, (double) std::pow (3.0f, (p[1] - 0.5f) * 2.0f));
        // SPLIT: cambio di velocità a metà
        {
            const double k = clampf ((p[2] - 0.5f) * 1.6f, -0.8f, 0.8f);
            const double first = 0.5 * (1.0 - k);               // porzione di campione suonata nella prima metà
            u = u < 0.5 ? u * 2.0 * first : first + (u - 0.5) * 2.0 * (1.0 - first);
        }
        // SWING: sposta il punto (nel tempo) in cui si raggiunge la metà del campione
        {
            const double ts = clampf (0.5f - (p[3] - 0.5f) * 0.8f, 0.1f, 0.9f);
            u = u < ts ? u / ts * 0.5 : 0.5 + (u - ts) / (1.0 - ts) * 0.5;
        }
        // MIRROR
        double m = p[0];
        if (m > 0.51)
        {
            const double speed = 1.0 + (m - 0.5) * 2.0 * 3.0;   // più veloce e rimbalza fra inizio e fine
            double x = u * speed;
            x = std::fmod (x, 2.0); u = x <= 1.0 ? x : 2.0 - x;
        }
        else if (m < 0.49)
        {
            const double loop = std::max (0.02, m * 2.0);        // fine regione avvicinata all'inizio, loop
            u = std::fmod (u, loop);
        }
        const double pos2 = reverse ? a - u * regionLen : a + u * regionLen;
        float L, R; readAt (s, pos2, L, R);
        t += step / regionLen * (m > 0.51 ? 1.0 : 1.0);
        if (t >= 1.0) playing = false;
        outL = L; outR = R;
    }

    // CLONER: banco di 8 bande con guadagni/decadimenti diversi a ogni colpo (approssimazione spettrale)
    void processCloner (const SampleData* s, float st, float en, float pitchSt, const float* p, float& outL, float& outR)
    {
        float L, R;
        const double step = (double) semis (pitchSt + (p[0] - 0.5f) * 2.0f + cloneDetune) * s->sourceRate / sr;
        if (! linearRead (s, st, en, step, L, R)) return;
        static const float centres[NB] = { 80, 160, 320, 640, 1280, 2560, 5120, 10240 };
        float yl = 0, yr = 0;
        const float tilt = (p[1] - 0.5f) * 2.0f;
        const float damp = (p[3] - 0.5f) * 2.0f;                 // >0 toglie risonanze, <0 le enfatizza
        for (int i = 0; i < NB; ++i)
        {
            const float q = damp < 0 ? 1.2f + (-damp) * 12.0f : 1.2f;
            bandL[i].set (centres[i], q, sr); bandR[i].set (centres[i], q, sr);
            bandL[i].process (L); bandR[i].process (R);
            if (damp > 0) bandEnv[i] *= std::exp (-damp * (float) (i + 1) * 6.0f / sr);
            const float tg = std::pow (2.0f, tilt * ((float) i - 3.5f) * 0.5f);
            const float g = bandGain[i] * bandEnv[i] * tg * (damp < 0 ? 1.0f / (1.0f + (-damp) * 4.0f) : 1.0f);
            yl += bandL[i].bpOut * g; yr += bandR[i].bpOut * g;
        }
        outL = yl * 1.1f; outR = yr * 1.1f;
    }

    // GRANULAR: pitch e velocità indipendenti
    void processGranular (const SampleData* s, float st, float en, float pitchSt, const float* p, float& outL, float& outR)
    {
        const int n = s->length();
        const double a = (double) clamp01 (st) * (n - 1), b = (double) clamp01 (en) * (n - 1);
        const double lo = std::min (a, b), hi = std::max (a, b), len = std::max (1.0, hi - lo);
        const float size = 0.005f * std::pow (60.0f, p[0]);     // 5 ms … 300 ms
        const float rate = 4.0f * std::pow (60.0f, p[1]);       // 4 … 240 grani/s
        const float speed = (p[2] - 0.5f) * 4.0f;               // -2 … +2 (0 = fermo all'inizio)
        // playhead
        playhead += (double) speed * s->sourceRate / sr;
        double ph = speed >= 0 ? lo + std::fmod (std::max (0.0, playhead), len) : hi - std::fmod (std::max (0.0, -playhead), len);
        // nuovi grani
        grainTimer += rate / sr;
        if (grainTimer >= 1.0f)
        {
            grainTimer -= 1.0f;
            for (auto& g : grains)
                if (! g.active)
                {
                    g.active = true; g.age = 0; g.len = std::max (16, (int) (size * sr));
                    double start = ph;
                    if (p[3] > 0.5f) start += rng.bi() * (p[3] - 0.5f) * 2.0f * len * 0.5;
                    g.pos = clampf ((float) start, (float) lo, (float) hi);
                    g.inc = (double) semis (pitchSt) * s->sourceRate / sr;
                    if (p[3] < 0.5f && rng.uni() < (0.5f - p[3]) * 2.0f) g.inc = -g.inc;   // grani al contrario
                    break;
                }
        }
        float L = 0, R = 0; int active = 0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            ++active;
            const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) g.age / (float) g.len);
            float gl, gr; readAt (s, g.pos, gl, gr);
            L += gl * w; R += gr * w;
            g.pos += g.inc; if (++g.age >= g.len) g.active = false;
        }
        const float norm = 1.0f / std::sqrt (std::max (1.0f, rate * size));
        outL = L * norm; outR = R * norm;
        // il granulare suona finché c'è l'inviluppo: si ferma dopo 30 s di sicurezza
        if (++granElapsed > (int) (sr * 30)) playing = false;
    }
};
} // namespace mp
