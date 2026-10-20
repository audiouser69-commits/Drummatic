#include "Params.h"

namespace mpp
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;

static juce::NormalisableRange<float> targetRange (int t)
{
    return { targetMap (t).lo, targetMap (t).hi,
             [t] (float, float, float n) { return denorm (t, n); },
             [t] (float, float, float v) { return norm (t, v); },
             [t] (float, float, float v) { return t == T_Pitch || t == T_SPitch ? std::round (v) : v; } };
}

static std::unique_ptr<APF> pct (const juce::String& id, const juce::String& name, float def, float lo = 0, float hi = 1)
{
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 1) + " %"; }));
}
static std::unique_ptr<APF> db (const juce::String& id, const juce::String& name, float lo, float hi, float def)
{
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; }));
}
static std::unique_ptr<APB> boolp (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
}
static std::unique_ptr<APC> choice (const juce::String& id, const juce::String& name, const juce::StringArray& c, int def)
{
    return std::make_unique<APC> (juce::ParameterID { id, 1 }, name, c, def);
}
static std::unique_ptr<API> intp (const juce::String& id, const juce::String& name, int lo, int hi, int def)
{
    return std::make_unique<API> (juce::ParameterID { id, 1 }, name, lo, hi, def);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto d = voiceDefaults (v);
        const juce::String pre = "V" + juce::String (v + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("voice" + juce::String (v + 1), "Voce " + juce::String (v + 1), "|");

        for (int t = 0; t < kNumTargets; ++t)
        {
            float def = targetMap (t).def;
            if (t == T_E1Decay) def = d.decay;
            if (t == T_Gain) def = d.gainDb;
            if (t == T_Pan) def = d.pan;
            if (t == T_Cutoff) def = 20000.0f;
            juce::String label = targetLabel (t);
            if (t >= T_P1 && t <= T_P4) label = "SYNTH " + juce::String (t - T_P1 + 1);
            if (t >= T_SP1 && t <= T_SP4) label = "SAMPLER " + juce::String (t - T_SP1 + 1);
            if (t == T_SPitch) label = "SAMPLER PITCH";
            if (t >= T_E1Attack && t <= T_E1Decay) label = "ENV1 " + label;
            if (t >= T_E2Attack && t <= T_E2Decay) label = "ENV2 " + label;
            if (t == T_L1Speed || t == T_L1Amp) label = "LFO1 " + label;
            if (t == T_L2Speed || t == T_L2Amp) label = "LFO2 " + label;
            group->addChild (std::make_unique<APF> (juce::ParameterID { vid (v, targetId (t)), 1 }, pre + label, targetRange (t), def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([t] (float x, int) { return formatTarget (t, x); })));
        }
        group->addChild (boolp (vid (v, "synthOn"), pre + "Synth On", true));
        group->addChild (choice (vid (v, "engine"), pre + "Engine", engineNames(), d.engine));
        group->addChild (boolp (vid (v, "env2"), pre + "Synth Env2", false));
        group->addChild (boolp (vid (v, "stereo"), pre + "Stereo", false));
        group->addChild (boolp (vid (v, "sampOn"), pre + "Sampler On", false));
        group->addChild (choice (vid (v, "sampEng"), pre + "Sampler Engine", samplerNames(), 0));
        group->addChild (choice (vid (v, "sampEnv"), pre + "Sampler Env", { "Off", "Env 1", "Env 2" }, 2));
        group->addChild (choice (vid (v, "fmode"), pre + "Filter Mode", fromList (filterModeNames(), kNumFilterModes), 0));
        group->addChild (boolp (vid (v, "post"), pre + "Dist Post", false));
        group->addChild (choice (vid (v, "dmode"), pre + "Dist Mode", fromList (distModeNames(), kNumDistModes), 0));
        for (int e = 1; e <= 2; ++e)
        {
            group->addChild (pct (vid (v, "e" + juce::String (e) + "curve"), pre + "Env" + juce::String (e) + " Curve", 1.0f));
            group->addChild (pct (vid (v, "e" + juce::String (e) + "vel"), pre + "Env" + juce::String (e) + " Velocity", 0.5f));
        }
        for (int l = 1; l <= 2; ++l)
        {
            const juce::String p = "l" + juce::String (l);
            group->addChild (boolp (vid (v, p + "sync"), pre + "LFO" + juce::String (l) + " Sync", true));
            group->addChild (boolp (vid (v, p + "bip"), pre + "LFO" + juce::String (l) + " Bipolar", true));
            group->addChild (choice (vid (v, p + "wave"), pre + "LFO" + juce::String (l) + " Algorithm", { "Wavetable", "Triangle Tilt", "Pulse Width", "Smooth Random" }, 0));
            group->addChild (choice (vid (v, p + "reset"), pre + "LFO" + juce::String (l) + " Reset", { "Free Run", "Always Reset", "Soft Reset", "Sample and Hold", "One Shot" }, 0));
            group->addChild (pct (vid (v, p + "shape"), pre + "LFO" + juce::String (l) + " Shape", 0.5f));
        }
        group->addChild (pct (vid (v, "macro1"), pre + "Macro 1", 0.0f));
        group->addChild (pct (vid (v, "macro2"), pre + "Macro 2", 0.0f));
        group->addChild (pct (vid (v, "variation"), pre + "Variation", 0.0f));
        group->addChild (choice (vid (v, "choke"), pre + "Choke", voiceTargets(), 0));
        group->addChild (choice (vid (v, "link"), pre + "Trigger", voiceTargets(), 0));
        group->addChild (choice (vid (v, "route"), pre + "Routing", { "Main 1-2", "Out 3-4", "Out 5-6", "Out 7-8", "Out 9-10", "Out 11-12", "Out 13-14", "Out 15-16" }, 0));
        group->addChild (boolp (vid (v, "pfEnable"), pre + "Performance Enable", true));
        group->addChild (boolp (vid (v, "pfFlip"), pre + "Performance Flip", false));
        // sequencer
        group->addChild (intp (vid (v, "steps"), pre + "Steps", 1, 64, 16));
        group->addChild (intp (vid (v, "fill"), pre + "Fill", 0, 64, d.fill));
        group->addChild (intp (vid (v, "pattern"), pre + "Pattern", 1, 64, 1));
        group->addChild (intp (vid (v, "rotate"), pre + "Rotate", -32, 32, d.rotate));
        group->addChild (boolp (vid (v, "invert"), pre + "Invert", false));
        group->addChild (pct (vid (v, "sshift"), pre + "Seq Shift", 0, -1, 1));
        group->addChild (pct (vid (v, "swing"), pre + "Swing", 0, -1, 1));
        group->addChild (pct (vid (v, "shape"), pre + "Seq Shape", 0, -1, 1));
        group->addChild (pct (vid (v, "density"), pre + "Density", 0, -1, 1));
        { juce::StringArray s; for (int i = 0; i < kNumSpeeds; ++i) s.add (speedName (i)); group->addChild (choice (vid (v, "speed"), pre + "Speed", s, 4)); }
        { juce::StringArray s; for (int i = 0; i < kNumMults; ++i) s.add (multName (i)); group->addChild (choice (vid (v, "mult"), pre + "Mult", s, 3)); }
        group->addChild (intp (vid (v, "length"), pre + "Length", 1, 128, 16));
        group->addChild (boolp (vid (v, "snap"), pre + "Snap", true));
        group->addChild (boolp (vid (v, "resetOn"), pre + "Reset", false));
        group->addChild (intp (vid (v, "resetLen"), pre + "Reset Length", 1, 128, 64));
        layout.add (std::move (group));
    }

    // ---- globali ----
    auto fx = std::make_unique<juce::AudioProcessorParameterGroup> ("fx", "FX / Master", "|");
    fx->addChild (boolp ("d_sync", "Delay Sync", true));
    fx->addChild (choice ("d_div", "Delay Time (sync)", lfoDivs(), 6));
    fx->addChild (std::make_unique<APF> (juce::ParameterID { "d_time", 1 }, "Delay Time (ms)", juce::NormalisableRange<float> (1, 2000, 0, 0.35f), 250,
                  juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatTime (v / 1000.0f); })));
    fx->addChild (pct ("d_fb", "Delay Feedback", 0.31f));
    fx->addChild (pct ("d_glitch", "Delay Glitch", 0.0f));
    fx->addChild (pct ("d_fbtilt", "Delay FB Tilt", 0.0f, -1, 1));
    fx->addChild (pct ("d_duck", "Delay Ducking", 0.0f));
    fx->addChild (pct ("d_srate", "Delay S.Rate", 1.0f));
    fx->addChild (db ("d_gain", "Delay Gain", -24, 12, 0));
    fx->addChild (boolp ("d_stereo", "Delay Stereo", true));
    fx->addChild (boolp ("d_pong", "Delay Pong", false));
    fx->addChild (pct ("d_torev", "Delay > Reverb", 0.0f));
    fx->addChild (boolp ("r_sync", "Reverb Predelay Sync", false));
    fx->addChild (choice ("r_div", "Reverb Predelay (sync)", lfoDivs(), 8));
    fx->addChild (std::make_unique<APF> (juce::ParameterID { "r_pre", 1 }, "Reverb Predelay", juce::NormalisableRange<float> (0.1f, 1000, 0, 0.3f), 1,
                  juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatTime (v / 1000.0f); })));
    fx->addChild (std::make_unique<APF> (juce::ParameterID { "r_decay", 1 }, "Reverb Decay", juce::NormalisableRange<float> (0.1f, 20, 0, 0.4f), 4,
                  juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatTime (v); })));
    fx->addChild (pct ("r_srate", "Reverb S.Rate", 1.0f));
    fx->addChild (pct ("r_intilt", "Reverb In Tilt", 0.0f, -1, 1));
    fx->addChild (pct ("r_outtilt", "Reverb Out Tilt", 0.0f, -1, 1));
    fx->addChild (db ("r_gain", "Reverb Gain", -24, 12, 0));
    fx->addChild (choice ("r_mode", "Reverb Mode", { "Dark", "Bright", "Hall", "Tin Can" }, 0));
    fx->addChild (pct ("m_max", "Maximize", 0.0f));
    fx->addChild (db ("m_low", "EQ Low", -18, 18, 0));
    fx->addChild (db ("m_mid", "EQ Mid", -18, 18, 0));
    fx->addChild (db ("m_high", "EQ High", -18, 18, 0));
    fx->addChild (db ("m_gain", "Master Gain", -36, 12, 0));
    fx->addChild (choice ("m_clip", "Clipping", { "No Clipping", "Soft Clip", "Hard Clip", "Wavefold" }, 1));
    fx->addChild (pct ("humanize", "Humanize", 0.0f));
    fx->addChild (std::make_unique<APF> (juce::ParameterID { "bpm", 1 }, "Tempo (standalone)", juce::NormalisableRange<float> (40, 300, 0.1f), 120,
                  juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " BPM"; })));
    fx->addChild (pct ("rnd_depth", "Randomize Depth", 0.5f));
    layout.add (std::move (fx));

    auto perf = std::make_unique<juce::AudioProcessorParameterGroup> ("perf", "Performance", "|");
    for (int i = 0; i < kNumPerf; ++i)
        perf->addChild (pct (perfId (i), juce::String ("Perf ") + perfLabel (i) + (i >= PF_SynPitch && i <= PF_Syn4 ? " (Synth)" : (i >= PF_SmpPitch && i <= PF_Smp4 ? " (Sampler)" : "")), 0.0f, -1, 1));
    perf->addChild (pct ("pf_depth", "Perf Depth", 1.0f, -1, 1));
    perf->addChild (boolp ("pf_enabled", "Perf Enabled", true));
    perf->addChild (pct ("pf_rnd", "Perf Randomize Depth", 0.1f));
    layout.add (std::move (perf));
    return layout;
}
} // namespace mpp
