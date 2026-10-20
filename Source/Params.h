#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/DrumMachine.h"

namespace mpp
{
using namespace mp;

// ID dei parametri modulabili (stesso ordine dell'enum Target)
inline const char* targetId (int t)
{
    static const char* ids[] = { "pitch", "p1", "p2", "p3", "p4", "spitch", "sp1", "sp2", "sp3", "sp4", "start", "end",
                                 "blend", "cutoff", "res", "distort", "e1a", "e1h", "e1d", "e2a", "e2h", "e2d",
                                 "l1spd", "l1amp", "l2spd", "l2amp", "pan", "gain", "dsend", "rsend" };
    return ids[t];
}
inline const char* targetLabel (int t)
{
    static const char* l[] = { "PITCH", "P1", "P2", "P3", "P4", "PITCH", "S1", "S2", "S3", "S4", "START", "END",
                               "BLEND", "CUTOFF", "RESONANCE", "DISTORT", "ATTACK", "HOLD", "DECAY", "ATTACK", "HOLD", "DECAY",
                               "SPEED", "AMP", "SPEED", "AMP", "PAN", "VOICE GAIN", "DELAY", "REVERB" };
    return l[t];
}

inline juce::String vid (int v, const juce::String& name) { return "v" + juce::String (v + 1) + "_" + name; }

// Testo dei valori
inline juce::String formatTime (float s) { return s < 0.01f ? juce::String (s * 1000.0f, 2) + " ms" : (s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s"); }
inline juce::String formatTarget (int t, float v)
{
    switch (t)
    {
        case T_Pitch: case T_SPitch: return juce::String (juce::roundToInt (v)) + " st";
        case T_Cutoff: return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        case T_E1Attack: case T_E1Hold: case T_E1Decay: case T_E2Attack: case T_E2Hold: case T_E2Decay: return formatTime (v);
        case T_Pan: return std::abs (v) < 0.005f ? juce::String ("Center") : (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100));
        case T_Gain: return v <= -59.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
        case T_DelaySend: case T_RevSend: return v <= 0.0005f ? juce::String ("-inf dB") : juce::String (20.0f * std::log10 (v * v), 1) + " dB";
        default: return juce::String (v * 100.0f, 1) + " %";
    }
}

struct VoiceDefaults { int engine; float decay; int fill; int rotate; float gainDb; float pan; };
inline VoiceDefaults voiceDefaults (int v)
{
    static const VoiceDefaults d[] = {
        { EngKick, 0.35f, 4, 0, -2, 0 }, { EngSnare, 0.22f, 2, 4, -4, 0 }, { EngHat, 0.05f, 8, 2, -9, 0.15f },
        { EngHat, 0.35f, 0, 0, -10, -0.15f }, { EngClap, 0.3f, 0, 0, -6, 0 }, { EngFM, 0.25f, 0, 0, -8, 0 },
        { EngModal, 0.6f, 0, 0, -10, 0.2f }, { EngCowbell, 0.2f, 0, 0, -12, -0.2f } };
    return d[v];
}

inline juce::StringArray engineNames()
{
    juce::StringArray a; for (int i = 0; i < kNumSynthEngines; ++i) a.add (engineInfo (i).name); return a;
}
inline juce::StringArray samplerNames()
{
    juce::StringArray a; for (int i = 0; i < kNumSamplerEngines; ++i) a.add (samplerInfo (i).name); return a;
}
inline juce::StringArray fromList (const char* const* l, int n) { juce::StringArray a; for (int i = 0; i < n; ++i) a.add (l[i]); return a; }
inline juce::StringArray voiceTargets() { juce::StringArray a { "---" }; for (int i = 1; i <= kNumVoices; ++i) a.add (juce::String (i)); return a; }
inline juce::StringArray lfoDivs() { juce::StringArray a; for (int i = 0; i < kNumLfoDivs; ++i) a.add (lfoDivName (i)); return a; }

// ---- Pagina PERFORMANCE ----
enum Perf
{
    PF_EnvA, PF_EnvH, PF_EnvD, PF_EnvC,
    PF_SynPitch, PF_Syn1, PF_Syn2, PF_Syn3, PF_Syn4,
    PF_SmpPitch, PF_Smp1, PF_Smp2, PF_Smp3, PF_Smp4,
    PF_Macro1, PF_Macro2, PF_Var, PF_ModAmp,
    PF_Distort, PF_Cutoff, PF_Res,
    PF_Steps, PF_Fill, PF_Pattern, PF_Density, PF_Prob, PF_Ratchet,
    kNumPerf
};
inline const char* perfId (int i)
{
    static const char* n[] = { "pf_enva", "pf_envh", "pf_envd", "pf_envc", "pf_spitch", "pf_s1", "pf_s2", "pf_s3", "pf_s4",
                               "pf_mpitch", "pf_m1", "pf_m2", "pf_m3", "pf_m4", "pf_macro1", "pf_macro2", "pf_var", "pf_modamp",
                               "pf_distort", "pf_cutoff", "pf_res", "pf_steps", "pf_fill", "pf_pattern", "pf_density", "pf_prob", "pf_ratchet" };
    return n[i];
}
inline const char* perfLabel (int i)
{
    static const char* n[] = { "ATTACK", "HOLD", "DECAY", "CURVE", "PITCH", "1", "2", "3", "4", "PITCH", "1", "2", "3", "4",
                               "MACRO 1", "MACRO 2", "VARIATION", "MOD AMP", "DISTORT", "CUTOFF", "RES",
                               "STEPS", "FILL", "PATTERN", "DENSITY", "PROB", "RATCHET" };
    return n[i];
}
// destinazione voce (Target) di un controllo Performance, o -1
inline int perfTarget (int i)
{
    switch (i)
    {
        case PF_SynPitch: return T_Pitch; case PF_Syn1: return T_P1; case PF_Syn2: return T_P2; case PF_Syn3: return T_P3; case PF_Syn4: return T_P4;
        case PF_SmpPitch: return T_SPitch; case PF_Smp1: return T_SP1; case PF_Smp2: return T_SP2; case PF_Smp3: return T_SP3; case PF_Smp4: return T_SP4;
        case PF_Distort: return T_Distort; case PF_Cutoff: return T_Cutoff; case PF_Res: return T_Res;
        default: return -1;
    }
}
inline float perfScale (int i) { return (i == PF_SynPitch || i == PF_SmpPitch) ? 0.25f : 1.0f; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace mpp
