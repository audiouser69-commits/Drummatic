#pragma once
#include "Common.h"

namespace mp
{
// Destinazioni di modulazione (spazio normalizzato 0..1). Aggiungere SOLO in fondo.
enum Target
{
    T_Pitch, T_P1, T_P2, T_P3, T_P4,
    T_SPitch, T_SP1, T_SP2, T_SP3, T_SP4, T_Start, T_End,
    T_Blend, T_Cutoff, T_Res, T_Distort,
    T_E1Attack, T_E1Hold, T_E1Decay, T_E2Attack, T_E2Hold, T_E2Decay,
    T_L1Speed, T_L1Amp, T_L2Speed, T_L2Amp,
    T_Pan, T_Gain, T_DelaySend, T_RevSend,
    // strato LAZER (0.4): le 12 manopole condivise dai 64 motori
    T_LzTune, T_LzSweep, T_LzSTime, T_LzTimbre, T_LzColor, T_LzMod, T_LzRate, T_LzGrit, T_LzDensity, T_LzSpread, T_LzDecay, T_LzLevel,
    kNumTargets
};
// Sorgenti di modulazione
enum Source { S_Random, S_Velocity, S_Macro1, S_Macro2, S_Env1, S_Env2, S_Lfo1, S_Lfo2, kNumSources };
inline const char* sourceName (int s)
{
    static const char* n[] = { "RANDOM", "VELOCITY", "MACRO 1", "MACRO 2", "ENV 1", "ENV 2", "LFO 1", "LFO 2" };
    return n[s];
}

struct Map { enum Kind { Lin, Exp }; float lo, hi; Kind kind; float def; };

inline const Map& targetMap (int t)
{
    static const Map m[] = {
        { -48, 48, Map::Lin, 0 },          // pitch (semitoni)
        { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f },
        { -48, 48, Map::Lin, 0 },          // pitch sampler
        { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f },
        { 0, 1, Map::Lin, 0 }, { 0, 1, Map::Lin, 1 },        // start / end
        { 0, 1, Map::Lin, 0.0f },          // blend (0 = solo synth)
        { 20, 20000, Map::Exp, 20000 },    // cutoff Hz
        { 0, 1, Map::Lin, 0 },             // resonance
        { 0, 1, Map::Lin, 0 },             // distort
        { 0.0005f, 5, Map::Exp, 0.001f }, { 0.0005f, 5, Map::Exp, 0.001f }, { 0.002f, 10, Map::Exp, 0.3f },     // env1 A H D (s)
        { 0.0005f, 5, Map::Exp, 0.001f }, { 0.0005f, 20, Map::Exp, 0.001f }, { 0.002f, 20, Map::Exp, 0.8f },   // env2
        { 0, 1, Map::Lin, 0.35f }, { 0, 1, Map::Lin, 1 },    // lfo1 speed (norm) / amp
        { 0, 1, Map::Lin, 0.35f }, { 0, 1, Map::Lin, 1 },    // lfo2
        { -1, 1, Map::Lin, 0 },            // pan
        { -60, 12, Map::Lin, 0 },          // gain dB
        { 0, 1, Map::Lin, 0 }, { 0, 1, Map::Lin, 0 },        // mandate delay / reverb
        { -24, 24, Map::Lin, 0 },          // LAZER tune (semitoni)
        { -1, 1, Map::Lin, -0.6f },        // sweep (±4 ottave, negativo = verso il basso)
        { 0.002f, 4, Map::Exp, 0.08f },    // tempo dello sweep (s)
        { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.3f }, { 0, 1, Map::Lin, 0.4f },  // timbre color mod rate
        { 0, 1, Map::Lin, 0 }, { 0, 1, Map::Lin, 0.5f }, { 0, 1, Map::Lin, 0.3f },                          // grit density spread
        { 0.005f, 20, Map::Exp, 0.35f },   // decay (s)
        { -60, 6, Map::Lin, -6 },          // level dB
    };
    return m[t];
}
inline float denorm (int t, float n)
{
    const Map& m = targetMap (t); n = clamp01 (n);
    return m.kind == Map::Lin ? m.lo + (m.hi - m.lo) * n : m.lo * std::pow (m.hi / m.lo, n);
}
inline float norm (int t, float v)
{
    const Map& m = targetMap (t);
    if (m.kind == Map::Lin) return clamp01 ((v - m.lo) / (m.hi - m.lo));
    return clamp01 (std::log (std::max (v, m.lo) / m.lo) / std::log (m.hi / m.lo));
}

// velocità LFO: normalizzato → Hz (non sincronizzato). 0..0.5 → 0.01..20 Hz, 0.5..1 → 20..440 Hz
inline float lfoHz (float n)
{
    n = clamp01 (n);
    return n < 0.5f ? 0.01f * std::pow (2000.0f, n * 2.0f) : 20.0f * std::pow (22.0f, (n - 0.5f) * 2.0f);
}
// divisioni sincronizzate (in battiti da 1/4)
constexpr int kNumLfoDivs = 13;
inline const char* lfoDivName (int i)
{
    static const char* n[] = { "8 bar", "4 bar", "2 bar", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/64" };
    return n[i];
}
inline float lfoDivBeats (int i)
{
    static const float b[] = { 32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 2.0f / 3, 1.0f / 3, 1.0f / 6, 0.0625f };
    return b[i];
}
inline int lfoDivIndex (float n) { return std::min (kNumLfoDivs - 1, (int) (clamp01 (n) * kNumLfoDivs)); }
} // namespace mp
