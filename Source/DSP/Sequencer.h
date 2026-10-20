#pragma once
#include "Common.h"
#include "Voice.h"
#include <atomic>
#include <functional>

namespace mp
{
constexpr int kMaxSteps = 128;
enum Lane { L_Vel, L_Shift, L_Pitch, L_Ratch, L_Env, L_Pan, L_Var, L_Prob, L_Sends, L_Macro2, kNumLanes };
enum LaneSync { LS_Sync, LS_Free, LS_Trig, LS_Retrig };

struct LaneInfo { const char* name; float lo, hi, def; bool integer; };
inline const LaneInfo& laneInfo (int l)
{
    static const LaneInfo li[] = {
        { "VEL",    0, 127, 100, true },
        { "SHIFT", -100, 100, 0, true },
        { "PITCH", -24, 24, 0, true },
        { "RATCH",  1, 8, 1, true },
        { "ENV",   -100, 100, 0, true },
        { "PAN",   -100, 100, 0, true },
        { "VAR",    0, 100, 0, true },
        { "PROB",  -100, 100, 0, true },
        { "SENDS", -100, 100, 0, true },
        { "MACRO 2", 0, 100, 0, true },
    };
    return li[l];
}

// Dati del sequencer condivisi con la UI (una per voce)
struct SeqShared
{
    std::atomic<bool> manual[kMaxSteps];
    std::atomic<float> lane[kNumLanes][kMaxSteps];
    std::atomic<int> laneLen[kNumLanes], laneSync[kNumLanes];
    std::atomic<int> playStep { -1 };          // per l'evidenziazione in UI
    SeqShared() { clearAll(); }
    void clearAll()
    {
        for (auto& m : manual) m = false;
        for (int l = 0; l < kNumLanes; ++l) { clearLane (l); laneLen[l] = 16; laneSync[l] = LS_Sync; }
    }
    void clearLane (int l) { for (auto& v : lane[l]) v = laneInfo (l).def; }
};

struct SeqSettings
{
    int steps = 16, fill = 0, pattern = 1, rotate = 0; bool invert = false;
    float shift = 0, swing = 0, shape = 0, density = 0;   // -1..1
    int speedIdx = 4, multIdx = 3, length = 16; bool snap = true, resetOn = false; int resetLen = 64;
    float probOffset = 0; int ratchOffset = 0;            // dalla pagina Performance
};

constexpr int kNumSpeeds = 10, kNumMults = 7;
inline const char* speedName (int i) { static const char* n[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T" }; return n[i]; }
inline float speedBeats (int i) { static const float b[] = { 4, 2, 1, 0.5f, 0.25f, 0.125f, 2.0f / 3, 1.0f / 3, 1.0f / 6, 1.0f / 12 }; return b[i]; }
inline const char* multName (int i) { static const char* n[] = { "/4", "/3", "/2", "1x", "2x", "3x", "4x" }; return n[i]; }
inline float multFactor (int i) { static const float f[] = { 0.25f, 1.0f / 3, 0.5f, 1, 2, 3, 4 }; return f[i]; }

inline int sequenceLength (const SeqSettings& s)
{
    const int steps = std::max (1, s.steps);
    if (! s.snap) return std::max (1, std::min (kMaxSteps, s.length));
    const int mult = std::max (1, (s.length + steps - 1) / steps);
    return std::min (kMaxSteps, mult * steps);
}

// Generatore di pattern euclideo + permutazioni
inline bool patternHit (const SeqSettings& s, int idx)
{
    const int steps = std::max (1, s.steps);
    const int fill = std::min (steps, std::max (0, s.fill));
    bool hits[kMaxSteps] {};
    for (int i = 0; i < steps; ++i) hits[i] = ((i * fill) % steps) < fill;
    if (s.pattern > 1 && fill > 0 && fill < steps)
    {
        // permutazione deterministica: sposta i colpi singolarmente
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < steps; ++i)
                if (hits[i])
                {
                    const float h = hashUni ((uint32_t) s.pattern, (uint32_t) i, (uint32_t) pass);
                    const int dir = h < 0.33f ? -1 : (h < 0.66f ? 0 : 1 + (int) (h * 3) % 2);
                    const int j = ((i + dir) % steps + steps) % steps;
                    if (dir != 0 && ! hits[j]) { hits[i] = false; hits[j] = true; }
                }
    }
    int k = ((idx - s.rotate) % steps + steps) % steps;
    const bool h = hits[k];
    return s.invert ? ! h : h;
}

struct SeqEvent { double beat; TriggerInfo info; };

class Sequencer
{
public:
    SeqShared* shared = nullptr;
    int voiceIndex = 0;

    void reset() { for (auto& c : laneCounter) c = 0; lastStep = -1; pending.clear(); pending.reserve (256); }

    // Trova gli eventi con battito in [b0, b1) e li passa a emit
    template <typename Emit>
    void collect (const SeqSettings& s, double b0, double b1, float humanize, uint32_t seed, Emit&& emit)
    {
        const double stepBeats = speedBeats (s.speedIdx) / multFactor (s.multIdx);
        if (stepBeats <= 0) return;
        const int steps = std::max (1, s.steps);
        const int L = sequenceLength (s);
        const long long nFirst = (long long) std::floor (b0 / stepBeats) - 2;
        const long long nLast = (long long) std::ceil (b1 / stepBeats) + 2;
        for (long long n = std::max (0LL, nFirst); n <= nLast; ++n)
        {
            const long long m = s.resetOn ? n % std::max (1, s.resetLen) : n;
            const int seqIdx = (int) (m % L);
            const int patIdx = (int) (m % steps);
            // tempo dell'evento
            const long long g0 = (n / steps) * steps;
            const float t = (float) (n - g0) / (float) steps;
            const float e = std::pow (3.0f, -clampf (s.shape, -1, 1));
            const float tw = std::pow (t, e);
            double beat = ((double) g0 + (double) tw * steps) * stepBeats;
            if (n % 2 == 1) beat += s.swing * 0.5 * stepBeats;
            beat -= s.shift * 0.5 * stepBeats;

            const bool manualOn = shared->manual[seqIdx].load();
            const bool active = manualOn || patternHit (s, patIdx);

            // aggiorna l'indice di riproduzione per la UI (solo sul passo "nominale")
            const double nominal = (double) n * stepBeats;
            if (nominal >= b0 && nominal < b1 && n != lastStep) { lastStep = n; shared->playStep = seqIdx; if (seqIdx == 0) retrigReset(); }

            // valori lane (letti senza avanzare ancora i contatori TRIG)
            float lv[kNumLanes];
            for (int l = 0; l < kNumLanes; ++l) lv[l] = shared->lane[l][laneIndex (l, m, L)].load();
            beat += lv[L_Shift] / 100.0 * 0.5 * stepBeats;
            if (humanize > 0)
                // condiviso fra i canali: dipende solo dalla griglia a 1/16, non dalla voce
                beat += (hashUni (seed, (uint32_t) std::llround (nominal * 4.0), 77u) - 0.5f) * humanize * humanize * 0.2;
            if (! (beat >= b0 && beat < b1)) continue;

            // probabilità: DENSITY e PROB
            const float r1 = hashUni (seed, (uint32_t) n, 1000u + (uint32_t) voiceIndex);
            const float r2 = hashUni (seed, (uint32_t) n, 2000u + (uint32_t) voiceIndex);
            bool fire = active;
            if (active && s.density < 0) fire = r1 < 1.0f + s.density;
            if (! active && s.density > 0) fire = r1 < s.density;
            const float prob = clampf (lv[L_Prob] / 100.0f + s.probOffset, -1, 1);
            int ratch = std::max (1, (int) std::round (lv[L_Ratch]) + s.ratchOffset);
            if (fire && prob < 0) fire = r2 < 1.0f + prob;
            if (fire && prob > 0 && r2 < prob) ratch *= 2;
            if (! fire) continue;

            // ora sì: avanza le lane TRIG/RETRIG e rileggi
            for (int l = 0; l < kNumLanes; ++l)
            {
                const int sm = shared->laneSync[l].load();
                if (sm == LS_Trig || sm == LS_Retrig)
                {
                    lv[l] = shared->lane[l][laneCounter[l] % std::max (1, shared->laneLen[l].load())].load();
                    ++laneCounter[l];
                }
            }
            TriggerInfo ti;
            ti.velocity = clamp01 (lv[L_Vel] / 127.0f);
            ti.accent = lv[L_Vel] >= 127.0f;
            ti.pitchSt = lv[L_Pitch];
            ti.envScale = std::pow (2.0f, lv[L_Env] / 100.0f * 2.0f);
            ti.pan = lv[L_Pan] / 100.0f;
            ti.varAmount = lv[L_Var] / 100.0f;
            const float snd = lv[L_Sends] / 100.0f;
            ti.sendDelay = std::max (0.0f, snd); ti.sendReverb = std::max (0.0f, -snd);
            ti.macro2 = lv[L_Macro2] > 0 ? lv[L_Macro2] / 100.0f : -1.0f;
            ratch = std::max (1, std::min (16, ratch));
            for (int j = 0; j < ratch; ++j)
            {
                const double rb = beat + j * stepBeats / ratch;
                if (rb < b1 || j == 0) emit (SeqEvent { rb, ti });
                else pending.push_back (SeqEvent { rb, ti });
            }
        }
        // ratchet che cadono nei blocchi successivi
        for (size_t i = 0; i < pending.size();)
        {
            if (pending[i].beat < b1) { if (pending[i].beat >= b0 - 1.0e-9) emit (pending[i]); pending.erase (pending.begin() + (long) i); }
            else if (pending[i].beat > b1 + 64) pending.erase (pending.begin() + (long) i);
            else ++i;
        }
    }

    void clearPending() { pending.clear(); }

private:
    int laneCounter[kNumLanes] {};
    long long lastStep = -1;
    std::vector<SeqEvent> pending;

    void retrigReset()
    {
        for (int l = 0; l < kNumLanes; ++l)
            if (shared->laneSync[l].load() == LS_Retrig) laneCounter[l] = 0;
    }
    int laneIndex (int l, long long m, int L) const
    {
        const int sm = shared->laneSync[l].load();
        if (sm == LS_Sync) return (int) (m % L);
        const int len = std::max (1, std::min (kMaxSteps, shared->laneLen[l].load()));
        if (sm == LS_Free) return (int) (m % len);
        return laneCounter[l] % len;
    }
};
} // namespace mp
