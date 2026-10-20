#pragma once
#include "Common.h"
#include <memory>

namespace mp
{
// Elenco motori di sintesi. ATTENZIONE: aggiungere i nuovi motori SOLO in fondo (compatibilità preset).
enum SynthEngineId
{
    EngKick, EngSnare, EngHat, EngClap, EngFM, EngModal, EngCowbell, EngZap, EngPillow,
    EngTripwire, EngRadiator, EngKombucha, EngLPG, EngBuilder, EngAdditive,
    EngChaos, EngVosim, EngSumSyn, EngReson, EngCombinatoric, EngTerraining, EngShame, EngEntangled, EngHaywire, EngScrapyard,
    kNumSynthEngines
};

struct EngineInfo { const char* name; const char* k[4]; bool octaveDown; };
inline const EngineInfo& engineInfo (int id)
{
    static const EngineInfo info[] = {
        { "Kick",      { "P. DECAY", "P. DEPTH", "WAVEFORM", "SUB" }, true },
        { "Snare",     { "P. DECAY", "P. DEPTH", "CUTOFF", "S. RATE" }, false },
        { "Hat",       { "BLEND", "S. RATE", "CUTOFF", "HARMONIC" }, false },
        { "Clap",      { "CLAPS", "SPACING", "CUTOFF", "REV DECAY" }, false },
        { "FM",        { "RATIO", "DEPTH", "FEEDBACK", "ENV" }, false },
        { "Modal",     { "HARMONICS", "DAMPEN", "VARY", "BRIGHT" }, false },
        { "Cowbell",   { "FILTER", "CLICK", "NOISE", "HARMONIC" }, false },
        { "Zap",       { "ENV", "CLICK", "OVERTONE", "SUB" }, true },
        { "Pillow",    { "PILLOW", "CLICK", "RATTLE", "SUB" }, true },
        { "Tripwire",  { "P. DECAY", "P. DEPTH", "TRANSIENT", "SQUARE" }, true },
        { "Radiator",  { "ENV", "TICK", "OVERTONE", "ROAST" }, true },
        { "Kombucha",  { "S. RATE", "FEEDBACK", "P. ENV", "DAMP" }, false },
        { "LPG",       { "FOLD", "SHAPE", "NOISE", "DAMP" }, false },
        { "Builder",   { "OSC WAVE", "OSC GAIN", "NOISE TILT", "NOISE GAIN" }, false },
        { "Additive",  { "FREQ RATIO", "AMP RATIO", "PARTIALS", "AMP ENV" }, false },
        { "Chaos",     { "RATIO", "CHAOS", "FILTER", "CHAOS ENV" }, false },
        { "VOSIM",     { "PARTIAL 1", "PARTIAL 2", "FM", "FM ENV" }, false },
        { "SumSyn",    { "RATIO", "BRIGHT", "PM", "PM ENV" }, false },
        { "Reson",     { "HARMONICS", "DAMPEN", "MATERIAL", "RING" }, false },
        { "Combinatoric", { "COMB RATIO", "FEEDBACK", "P. ENV", "FM" }, false },
        { "Terraining", { "PITCH 2", "SHAPE 1", "SHAPE 2", "TERRAIN" }, false },
        { "Shame",     { "PITCH 2", "FM", "FEEDBACK", "SHAME" }, false },
        { "Entangled", { "PITCH 2", "FM", "FEEDBACK", "TANGLE" }, false },
        { "Haywire",   { "OVERTONE", "P. DEPTH", "BRIGHT", "RATTLE" }, false },
        { "Scrapyard", { "RATIO", "ZAP", "MATERIAL", "FM" }, false },
    };
    return info[std::min (std::max (id, 0), (int) kNumSynthEngines - 1)];
}

struct Engine
{
    float sr = 48000; Rng rng;
    virtual ~Engine() = default;
    virtual void prepare (float sampleRate) { sr = sampleRate; }
    virtual void trigger (float vel) = 0;
    virtual float process (float freq, const float* p) = 0;
    float noise() { return rng.bi(); }
};

// ---- Oscillatore con rampa di fase ----
struct Phasor
{
    double ph = 0;
    float tick (float hz, float sr) { ph += (double) hz / sr; ph -= std::floor (ph); return (float) ph; }
    void reset (double v = 0) { ph = v; }
};
inline float sineOf (float ph) { return std::sin (kTwoPi * ph); }

// ===================== KICK =====================
struct KickEngine : Engine
{
    Phasor ph, sub; DecayEnv penv;
    void trigger (float) override { ph.reset (0.25); sub.reset (0.25); penv.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.003f + std::pow (p[0], 2.0f) * 0.25f, sr);
        const float e = penv.next();
        const float hz = f * (1.0f + p[1] * 14.0f * e * e);
        const float x = ph.tick (hz, sr);
        const float s = sineOf (x);
        // WAVEFORM: sine → triangolo → quadra smussata
        const float w = p[2];
        float shaped;
        if (w < 0.5f)
        {
            const float tri = 4.0f * std::abs (std::fmod (x + 0.75f, 1.0f) - 0.5f) - 1.0f; // in fase col seno
            shaped = lerp (tri, s, w * 2.0f);
        }
        else shaped = fastTanh (s * (1.0f + (w - 0.5f) * 14.0f));
        const float sb = sineOf (sub.tick (hz * 0.5f, sr)) * p[3];
        return shaped * 0.9f + sb * 0.8f;
    }
};

// ===================== SNARE =====================
struct SnareEngine : Engine
{
    Phasor ph; DecayEnv penv, nenv; Svf hp, lp; float held = 0, acc = 0;
    void trigger (float) override { ph.reset (0); penv.trigger(); nenv.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.002f + p[0] * p[0] * 0.08f, sr);
        nenv.setTime (0.12f, sr);
        const float e = penv.next();
        const float tone = sineOf (ph.tick (f * 1.4f * (1.0f + p[1] * 3.0f * e), sr));
        // rumore a sample-rate ridotto
        const float rate = lerp (0.03f, 1.0f, p[3]);
        acc += rate; if (acc >= 1.0f) { acc -= 1.0f; held = noise(); }
        const float c = 400.0f * std::pow (40.0f, p[2]);
        hp.set (c * 0.5f, 0.7f, sr); lp.set (std::min (c * 6.0f, sr * 0.45f), 0.7f, sr);
        hp.process (held); lp.process (hp.hpOut);
        return tone * 0.65f + lp.lpOut * 0.9f * (0.3f + 0.7f * nenv.next());
    }
};

// ===================== HAT (stile 808) =====================
struct HatEngine : Engine
{
    float ph[6] {}; Svf bp, hp; float held = 0, acc = 0; Rng rng2 { 0x5151 };
    void trigger (float) override {}
    float process (float f, const float* p) override
    {
        static const float r808[6] = { 1.0f, 1.483f, 1.80f, 2.546f, 2.630f, 3.896f };
        float sq = 0;
        const float harm = p[3];
        for (int i = 0; i < 6; ++i)
        {
            const float ratio = lerp (1.0f + (float) i * 0.5f, r808[i], clamp01 (harm * 2.0f)) * (1.0f + std::max (0.0f, harm - 0.5f) * (float) i * 0.4f);
            ph[i] += f * 1.55f * ratio / sr; ph[i] -= std::floor (ph[i]);
            sq += ph[i] < 0.5f ? 1.0f : -1.0f;
        }
        sq *= (1.0f / 6.0f);
        const float rate = lerp (0.05f, 1.0f, p[1]);
        acc += rate; if (acc >= 1.0f) { acc -= 1.0f; held = noise(); }
        const float c = 2000.0f * std::pow (8.0f, p[2]);
        bp.set (c, 2.0f, sr); hp.set (c * 0.8f, 0.7f, sr);
        bp.process (sq); hp.process (held);
        return lerp (bp.bpOut * 2.5f, hp.hpOut, p[0]);
    }
};

// ===================== CLAP =====================
struct ClapEngine : Engine
{
    int burstsLeft = 0, counter = 0; DecayEnv burst, tail; Svf bp; OnePole lp;
    void trigger (float) override { burstsLeft = 1; counter = 0; burst.trigger(); tail.v = 0; }
    float process (float, const float* p) override
    {
        const int claps = 1 + (int) std::round (p[0] * 7.0f);
        const int spacing = (int) (sr * (0.002f + p[1] * p[1] * 0.06f));
        burst.setTime (0.0035f, sr);
        tail.setTime (0.02f + p[3] * p[3] * 1.2f, sr);
        if (burstsLeft > 0 && ++counter >= spacing)
        {
            counter = 0;
            if (burstsLeft < claps) { ++burstsLeft; burst.trigger(); }
            else { burstsLeft = 0; tail.trigger (0.6f); }
        }
        const float c = 600.0f * std::pow (12.0f, p[2]);
        bp.set (c, 1.5f, sr); lp.setCutoff (c * 2.5f, sr);
        const float n = noise();
        bp.process (n * (burst.next() + tail.next()));
        return lp.lp (bp.bpOut) * 3.0f;
    }
};

// ===================== FM 2 operatori =====================
struct FmEngine : Engine
{
    Phasor car, mod; DecayEnv env; float last = 0;
    void trigger (float) override { env.trigger(); }
    float process (float f, const float* p) override
    {
        // RATIO quantizzato morbido verso rapporti interi/mezzi
        const float r = std::pow (2.0f, lerp (-2.0f, 4.0f, p[0]));
        const float envLevel = p[3];
        env.setTime (0.005f + (1.0f - envLevel) * 0.3f + 0.02f, sr);
        const float e = env.next() * envLevel * 4.0f;
        const float depth = p[1] * 6.0f + e;
        const float fb = std::min (1.6f, p[2] * 1.4f + e * 0.2f);
        const float m = sineOf (mod.tick (f * r, sr));
        const float cp = car.tick (f, sr);
        const float out = std::sin (kTwoPi * cp + depth * m + fb * last);
        last = 0.5f * (last + out);
        return out * 0.85f;
    }
};

// ===================== MODAL =====================
struct ModalEngine : Engine
{
    static constexpr int N = 8;
    Phasor ph[N]; float amp[N] {}, coef[N] {};
    float lastDampen = -1;
    void trigger (float) override
    {
        for (int i = 0; i < N; ++i) { amp[i] = 1.0f; ph[i].reset(); }
        varyPending = true;
    }
    bool varyPending = false;
    float process (float f, const float* p) override
    {
        static const float bar[N]  = { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.8f, 31.9f };
        static const float memb[N] = { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f };
        if (varyPending)
        {
            for (int i = 0; i < N; ++i) amp[i] = 1.0f - p[2] * rng.uni();
            varyPending = false;
        }
        float out = 0;
        const float stretch = lerp (0.6f, 1.6f, p[3]);
        for (int i = 0; i < N; ++i)
        {
            const float rr = lerp (memb[i], bar[i], p[0]);
            const float ratio = 1.0f + (rr - 1.0f) * stretch;
            const float hz = f * 2.0f * ratio;
            if (hz > sr * 0.45f) continue;
            const float dec = 1.6f / (1.0f + (float) i * p[1] * 6.0f);
            const float c = std::exp (-1.0f / (dec * sr));
            out += sineOf (ph[i].tick (hz, sr)) * amp[i] / (1.0f + (float) i * 0.6f);
            amp[i] *= c;
        }
        return out * 0.55f;
    }
};

// ===================== COWBELL =====================
struct CowbellEngine : Engine
{
    float p1 = 0, p2 = 0; Svf hp, bp; DecayEnv click;
    void trigger (float) override { click.trigger(); }
    float process (float f, const float* p) override
    {
        const float ratio = lerp (1.2f, 2.0f, p[3]);
        const float base = f * 4.1f;
        p1 += base / sr; p1 -= std::floor (p1);
        p2 += base * ratio / sr; p2 -= std::floor (p2);
        float x = ((p1 < 0.5f ? 1.0f : -1.0f) + (p2 < 0.5f ? 1.0f : -1.0f)) * 0.5f;
        x += noise() * p[2] * 0.8f;
        click.setTime (0.002f, sr);
        x += noise() * click.next() * p[1] * 3.0f;
        hp.set (100.0f * std::pow (40.0f, p[0]), 0.7f, sr); hp.process (x);
        bp.set (base * 1.4f, 1.2f, sr); bp.process (hp.hpOut);
        return (bp.bpOut * 1.6f + hp.hpOut * 0.3f);
    }
};

// ===================== ZAP =====================
struct ZapEngine : Engine
{
    Phasor ph, ov, sub; DecayEnv penv, click; float held = 0; int cnt = 0;
    void trigger (float) override { ph.reset (0.25); penv.trigger(); click.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.02f + p[0] * 0.25f, sr);
        const float e = penv.next();
        const float hz = f * (1.0f + p[0] * 60.0f * e * e);
        const float main = sineOf (ph.tick (hz, sr));
        const float over = sineOf (ov.tick (hz * 4.0f, sr));
        click.setTime (0.004f, sr);
        if (++cnt >= 6) { cnt = 0; held = noise(); }      // transiente lo-fi
        const float c = held * click.next() * p[1] * 2.0f;
        const float sb = sineOf (sub.tick (hz * 0.5f, sr)) * p[3];
        return lerp (main, over, p[2] * 0.7f) + c + sb * 0.8f;
    }
};

// ===================== PILLOW =====================
struct PillowEngine : Engine
{
    Phasor ph, sub; DecayEnv penv, click; PinkNoise pink; Svf lp1, lp2;
    void trigger (float) override { ph.reset (0.25); penv.trigger(); click.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.03f, sr);
        const float e = penv.next();
        const float hz = f * (1.0f + 2.5f * e * e);
        const float s = sineOf (ph.tick (hz, sr));
        const float pn = pink.process (noise());
        const float x = lerp (s, s * pn * 4.0f, p[2]);
        click.setTime (0.0025f, sr);
        const float c = noise() * click.next() * p[1] * 1.5f;
        const float sb = sineOf (sub.tick (hz * 0.5f, sr)) * p[3];
        const float cut = 60.0f * std::pow (200.0f, p[0]);
        lp1.set (cut, 0.6f, sr); lp2.set (cut, 0.6f, sr);
        lp1.process (x + c + sb * 0.8f); lp2.process (lp1.lpOut);
        return lp2.lpOut * 1.3f;
    }
};

// ===================== TRIPWIRE =====================
struct TripwireEngine : Engine
{
    Phasor ph; DecayEnv e1, e2, tr;
    void trigger (float) override { ph.reset (0.25); e1.trigger(); e2.trigger(); tr.trigger(); }
    float process (float f, const float* p) override
    {
        e1.setTime (0.002f + p[0] * p[0] * 0.08f, sr);
        e2.setTime (0.01f + p[0] * 0.4f, sr);
        const float a = e1.next(), b = e2.next();
        const float hz = f * (1.0f + p[1] * (20.0f * a * a + 3.0f * b * b * b));
        const float s = sineOf (ph.tick (hz, sr));
        const float sq = fastTanh (s * (1.0f + p[3] * 20.0f)) / fastTanh (1.0f + p[3] * 20.0f);
        tr.setTime (0.0015f, sr);
        return lerp (s, sq, p[3]) + noise() * tr.next() * p[2] * 2.0f;
    }
};

// ===================== RADIATOR =====================
struct RadiatorEngine : Engine
{
    Phasor ph, ov, sub; DecayEnv penv, tick; OnePole lp;
    void trigger (float) override { ph.reset (0.25); penv.trigger(); tick.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.01f + p[0] * 0.12f, sr);
        const float e = penv.next();
        const float hz = f * (1.0f + p[0] * 12.0f * e * e);
        const float s = fastTanh (sineOf (ph.tick (hz, sr)) * 2.5f);
        const float o = sineOf (ov.tick (hz * 4.0f, sr));
        tick.setTime (0.003f, sr);
        const float t = noise() * tick.next() * p[1] * 2.5f;
        lp.setCutoff (300.0f, sr);
        const float r = sineOf (sub.tick (hz * 0.5f, sr)) * (0.4f + lp.lp (noise()) * 6.0f) * p[3];
        return fastTanh ((lerp (s, o, p[2]) + t + r) * 1.4f);
    }
};

// ===================== KOMBUCHA (Karplus-Strong) =====================
struct KombuchaEngine : Engine
{
    DelayLine dl; DecayEnv burst, penv; OnePole damp; float held = 0, acc = 0;
    void prepare (float s) override { Engine::prepare (s); dl.init ((int) (s / 20.0f) + 8); }
    void trigger (float) override { burst.trigger(); penv.trigger(); }
    float process (float f, const float* p) override
    {
        burst.setTime (0.004f, sr); penv.setTime (0.03f, sr);
        const float rate = lerp (0.03f, 1.0f, p[0]);
        acc += rate; if (acc >= 1.0f) { acc -= 1.0f; held = noise(); }
        const float ex = held * burst.next();
        const float hz = f * 2.0f * (1.0f + p[2] * 3.0f * penv.next());
        const float d = sr / std::max (20.0f, hz);
        const float fb = lerp (0.2f, 0.998f, std::pow (p[1], 0.4f));
        damp.setCutoff (20000.0f * std::pow (0.02f, p[3]), sr);
        const float y = ex + fb * damp.lp (dl.read (d));
        dl.push (fastTanh (y));
        return y * 0.9f;
    }
};

// ===================== LPG (bongo modulare) =====================
struct LpgEngine : Engine
{
    Phasor ph; DecayEnv env; Svf lp; float velo = 1;
    void trigger (float vel) override { env.trigger(); velo = vel; ph.reset (0.25); }
    float process (float f, const float* p) override
    {
        env.setTime (0.08f + p[3] * 0.4f, sr);
        const float e = env.next();
        float x = sineOf (ph.tick (f * 2.0f, sr));
        float v = x * (1.0f + p[0] * 6.0f);                      // wavefold
        for (int i = 0; i < 4 && (v > 1.0f || v < -1.0f); ++i) v = v > 1.0f ? 2.0f - v : -2.0f - v;
        v = lerp (v, fastTanh (v * 4.0f), p[1]);                  // waveshape
        v = lerp (v, noise(), p[2]);
        const float cut = 80.0f + 16000.0f * e * e * lerp (0.3f, 1.0f, velo) * lerp (0.2f, 1.0f, p[3]);
        lp.set (cut, 0.9f, sr); lp.process (v);
        return lp.lpOut * (0.3f + 0.7f * e) * 1.2f;
    }
};

// ===================== BUILDER =====================
struct BuilderEngine : Engine
{
    Phasor ph; Svf nf;
    void trigger (float) override { ph.reset (0); }
    float process (float f, const float* p) override
    {
        const float x = ph.tick (f, sr);
        const float sine = sineOf (x), tri = 2.0f * std::abs (2.0f * x - 1.0f) - 1.0f;
        const float saw = 2.0f * x - 1.0f, sq = x < 0.5f ? 1.0f : -1.0f;
        const float w = p[0] * 3.0f;
        const float osc = w < 1 ? lerp (sine, tri, w) : (w < 2 ? lerp (tri, saw, w - 1) : lerp (saw, sq, w - 2));
        // NOISE TILT: <0.5 passa-basso, >0.5 passa-alto
        const float n = noise();
        float nt;
        if (p[2] < 0.5f) { nf.set (200.0f * std::pow (100.0f, p[2] * 2.0f), 0.7f, sr); nf.process (n); nt = nf.lpOut; }
        else { nf.set (50.0f * std::pow (200.0f, (p[2] - 0.5f) * 2.0f), 0.7f, sr); nf.process (n); nt = nf.hpOut; }
        return osc * p[1] + nt * p[3];
    }
};

// ===================== ADDITIVE =====================
struct AdditiveEngine : Engine
{
    static constexpr int N = 24; Phasor ph[N]; DecayEnv env;
    void trigger (float) override { env.trigger(); for (auto& x : ph) x.reset(); }
    float process (float f, const float* p) override
    {
        const float fr = lerp (0.5f, 2.5f, p[0]);
        env.setTime (0.01f + p[3] * 0.5f, sr);
        const float ar = clamp01 (p[1] + env.next() * p[3] * 0.5f);
        const int parts = 1 + (int) (p[2] * (N - 1));
        float out = 0, norm = 0;
        for (int i = 0; i < parts; ++i)
        {
            const float hz = f * (1.0f + (float) i * fr);
            if (hz > sr * 0.45f) break;
            const float a = std::pow (lerp (0.2f, 0.98f, ar), (float) i);
            out += sineOf (ph[i].tick (hz, sr)) * a; norm += a;
        }
        out /= std::max (1.0f, norm * 0.6f);
        return ar > 0.85f ? fastTanh (out * (1.0f + (ar - 0.85f) * 20.0f)) : out;
    }
};

} // namespace mp
#include "SynthEngines2.h"
namespace mp
{
inline std::unique_ptr<Engine> makeEngine (int id)
{
    switch (id)
    {
        case EngKick: return std::make_unique<KickEngine>();
        case EngSnare: return std::make_unique<SnareEngine>();
        case EngHat: return std::make_unique<HatEngine>();
        case EngClap: return std::make_unique<ClapEngine>();
        case EngFM: return std::make_unique<FmEngine>();
        case EngModal: return std::make_unique<ModalEngine>();
        case EngCowbell: return std::make_unique<CowbellEngine>();
        case EngZap: return std::make_unique<ZapEngine>();
        case EngPillow: return std::make_unique<PillowEngine>();
        case EngTripwire: return std::make_unique<TripwireEngine>();
        case EngRadiator: return std::make_unique<RadiatorEngine>();
        case EngKombucha: return std::make_unique<KombuchaEngine>();
        case EngLPG: return std::make_unique<LpgEngine>();
        case EngBuilder: return std::make_unique<BuilderEngine>();
        case EngAdditive: return std::make_unique<AdditiveEngine>();
        case EngChaos: return std::make_unique<ChaosEngine>();
        case EngVosim: return std::make_unique<VosimEngine>();
        case EngSumSyn: return std::make_unique<SumSynEngine>();
        case EngReson: return std::make_unique<ResonEngine>();
        case EngCombinatoric: return std::make_unique<CombinatoricEngine>();
        case EngTerraining: return std::make_unique<TerrainEngine>();
        case EngShame: return std::make_unique<ShameEngine>();
        case EngEntangled: return std::make_unique<EntangledEngine>();
        case EngHaywire: return std::make_unique<HaywireEngine>();
        case EngScrapyard: return std::make_unique<ScrapyardEngine>();
        default: return std::make_unique<KickEngine>();
    }
}
} // namespace mp
