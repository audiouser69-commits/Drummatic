// Crea il kit dimostrativo "Drummatic Demo": un campione sintetico, il kit .mkit
// e (opzionale, argomento "--standalone") lo stato iniziale dell'app standalone.
#include "../Source/PluginProcessor.h"
#include <cstdio>

using namespace mp;

static juce::File makeDemoSample (const juce::File& dir)
{
    dir.createDirectory();
    auto f = dir.getChildFile ("Drummatic Metal Hit.wav");
    const int sr = 44100, n = sr;
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float env = std::exp (-t * 6.0f);
        float x = 0;
        const float partials[] = { 220.0f, 563.0f, 931.0f, 1377.0f, 2210.0f };
        for (int k = 0; k < 5; ++k) x += std::sin (6.2831853f * partials[k] * t) * std::exp (-t * (3.0f + k * 4.0f)) / (1.0f + k);
        x += (juce::Random::getSystemRandom().nextFloat() * 2 - 1) * std::exp (-t * 60.0f) * 0.4f;
        b.setSample (0, i, x * env * 0.5f);
        b.setSample (1, i, x * env * 0.5f * (1.0f - 0.2f * std::sin (t * 9)));
    }
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto out = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0)))
        out->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    ManipoloProcessor p;
    auto set = [&] (int v, const char* n, float val) { p.setParam (v, n, val); };
    auto lane = [&] (int v, int l, std::initializer_list<float> vals)
    {
        int i = 0; for (float x : vals) p.dm.sshared[v].lane[l][i++] = x;
    };

    // 1 Kick
    set (0, "engine", EngKick); set (0, "fill", 4); set (0, "e1d", 0.42f); set (0, "p2", 0.6f); set (0, "p4", 0.35f); set (0, "gain", -3);
    // 2 Snare acustico (Haywire)
    set (1, "engine", EngHaywire); set (1, "fill", 2); set (1, "rotate", 4); set (1, "e1d", 0.25f); set (1, "rsend", 0.45f); set (1, "gain", -5);
    set (1, "dmode", Tape); set (1, "distort", 0.25f);
    // 3 Hat chiuso
    set (2, "engine", EngHat); set (2, "fill", 11); set (2, "e1d", 0.045f); set (2, "swing", 0.25f); set (2, "gain", -11); set (2, "pan", 0.2f);
    set (2, "choke", 4); set (2, "variation", 0.25f);
    lane (2, L_Vel, { 127, 70, 90, 60, 110, 70, 95, 60, 127, 70, 90, 60, 110, 70, 100, 80 });
    // 4 Hat aperto
    set (3, "engine", EngHat); set (3, "fill", 2); set (3, "rotate", 2); set (3, "e1d", 0.35f); set (3, "gain", -13); set (3, "pan", -0.2f); set (3, "choke", 3);
    // 5 FM percussivo, poliritmo 3 su 7
    set (4, "engine", EngFM); set (4, "steps", 7); set (4, "fill", 3); set (4, "snap", 0); set (4, "length", 7);
    set (4, "p1", 0.7f); set (4, "p2", 0.45f); set (4, "e1d", 0.18f); set (4, "dsend", 0.5f); set (4, "gain", -9);
    set (4, "cutoff", 4000.0f); set (4, "res", 0.35f);
    p.dm.vshared[4].depth[S_Lfo1][T_Cutoff] = 0.3f;
    p.dm.vshared[4].depth[S_Random][T_P1] = 0.25f;
    // 6 Modal, 5 su 13 con permutazione
    set (5, "engine", EngModal); set (5, "steps", 13); set (5, "fill", 5); set (5, "pattern", 3); set (5, "snap", 0); set (5, "length", 13);
    set (5, "e1d", 0.9f); set (5, "dsend", 0.35f); set (5, "rsend", 0.35f); set (5, "gain", -12); set (5, "pan", 0.3f);
    lane (5, L_Pitch, { 0, 7, 3, 12, 5, 0, 10, 3, 7, 0, 15, 5, 3 });
    // 7 Sampler granulare + Scrapyard
    auto sample = makeDemoSample (ManipoloProcessor::userFolder().getChildFile ("Samples"));
    p.loadSample (6, sample);
    set (6, "engine", EngScrapyard); set (6, "sampEng", SmpGranular); set (6, "blend", 0.5f); set (6, "fill", 3); set (6, "steps", 16); set (6, "rotate", 5);
    set (6, "sp1", 0.35f); set (6, "sp2", 0.55f); set (6, "sp3", 0.7f); set (6, "sp4", 0.6f); set (6, "e2d", 1.2f); set (6, "rsend", 0.3f); set (6, "gain", -10);
    set (6, "pan", -0.3f);
    p.dm.vshared[6].depth[S_Velocity][T_Cutoff] = 0.4f;
    p.dm.vshared[6].locked[T_Pitch] = true;
    // 8 Kombucha, linea melodica a ratchet
    set (7, "engine", EngKombucha); set (7, "fill", 6); set (7, "pattern", 5); set (7, "e1d", 0.5f); set (7, "p2", 0.75f); set (7, "pitch", -12);
    set (7, "dsend", 0.3f); set (7, "gain", -9);
    lane (7, L_Pitch, { 0, 3, 7, 10, 12, 10, 7, 3, 0, -2, 3, 7, 5, 3, 0, -5 });
    lane (7, L_Ratch, { 1, 1, 1, 2, 1, 1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1 });

    if (argc > 2 && juce::String (argv[2]) == "--sampler")
    {
        const auto smp = ManipoloProcessor::userFolder().getChildFile ("Samples");
        // 1: break affettato con MACRO 2 -> START
        p.resetVoice (0);
        p.loadSample (0, smp.getChildFile ("Drummatic Break 120.wav"));
        set (0, "synthOn", 0); set (0, "blend", 1.0f); set (0, "sampEnv", 2); set (0, "e2d", 0.25f); set (0, "e2h", 0.02f);
        set (0, "fmode", DarkLP); set (0, "fill", 16); set (0, "steps", 16); set (0, "gain", -4);
        set (0, "sp3", 0.6f); set (0, "sp4", 0.55f);
        p.dm.vshared[0].depth[S_Macro2][T_Start] = 0.96f;
        lane (0, L_Macro2, { 0, 13, 26, 39, 52, 13, 78, 91, 0, 39, 26, 65, 52, 91, 78, 91 });
        lane (0, L_Ratch, { 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 1 });
        // 2: pad granulare
        p.resetVoice (1);
        p.loadSample (1, smp.getChildFile ("Drummatic Pad A.wav"));
        set (1, "synthOn", 0); set (1, "blend", 1.0f); set (1, "sampEng", SmpGranular); set (1, "sampEnv", 2);
        set (1, "e2a", 0.15f); set (1, "e2d", 2.5f); set (1, "sp1", 0.55f); set (1, "sp2", 0.6f); set (1, "sp3", 0.58f); set (1, "sp4", 0.7f);
        set (1, "fmode", DarkLP); set (1, "fill", 3); set (1, "rotate", 2); set (1, "rsend", 0.5f); set (1, "start", 0.2f);
        lane (1, L_Pitch, { 0, 0, 7, 0, 0, 12, 0, 0, 5, 0, 0, 3, 0, 0, 0, 0 });
        p.dm.vshared[1].depth[S_Lfo1][T_SP3] = 0.15f;
        // 3: hat aperto con Cloner
        p.resetVoice (2);
        p.loadSample (2, smp.getChildFile ("Drummatic Open Hat.wav"));
        set (2, "synthOn", 0); set (2, "blend", 1.0f); set (2, "sampEng", SmpCloner); set (2, "sampEnv", 0);
        set (2, "sp3", 0.7f); set (2, "sp4", 0.6f); set (2, "fmode", DarkLP); set (2, "fill", 11); set (2, "gain", -12);
        // 4: kick synth + campione stratificati
        p.resetVoice (3);
        p.loadSample (3, smp.getChildFile ("Drummatic Metal Hit.wav"));
        set (3, "engine", EngKick); set (3, "blend", 0.5f); set (3, "sampEng", SmpPhaseWarp); set (3, "sampEnv", 1);
        set (3, "sp1", 0.75f); set (3, "sp2", 0.35f); set (3, "fill", 4); set (3, "e1d", 0.4f); set (3, "gain", -6);
        set (3, "start", 0.05f); set (3, "end", 0.6f);
    }

    // 0.4: strato LAZER sulla voce 6 (Laser Bounce con BRK TAPZ), tappeto Grain Cloud sotto la voce 8, coda String sulla 5,
    // REPEAT sulla 2 e una performance di glitch programmata nel sequencer della Performance
    set (5, "lzOn", 1); set (5, "lzEng", 6); set (5, "lzsweep", -0.5f); set (5, "lzstime", 0.25f); set (5, "lztimbre", 0.35f);
    set (5, "lzlevel", -10.0f); set (5, "tzOn", 1); set (5, "tzTaps", 9); set (5, "tzShape", 0.6f); set (5, "tzMix", 0.5f);
    set (7, "lzOn", 1); set (7, "lzEng", 15); set (7, "lzdecay", 6.0f); set (7, "lzspread", 0.9f); set (7, "lzlevel", -16.0f); set (7, "lzdens", 0.35f);
    p.dm.vshared[7].depth[S_Lfo1][T_LzColor] = 0.3f;
    set (4, "tail", 3); set (4, "tailLvl", 0.4f); set (4, "tailDec", 0.8f);
    set (1, "rpCount", 3); set (1, "rpGaps", 0.3f); set (1, "rpRev", 0.35f); set (1, "rpCrush", 0.3f);
    p.apvts.getParameter ("ps_on")->setValueNotifyingHost (1.0f);
    {
        auto& s = p.perfSeq;
        s.lock[3][mpp::PF_RpMix] = 1.0f; s.lock[3][mpp::PF_RpGaps] = 0.4f;
        s.lock[6][mpp::PF_LzEngine] = 0.25f; s.lock[6][mpp::PF_LzTimbre] = 0.6f; s.lock[6][mpp::PF_TzMix] = 0.9f;
        s.lock[7][mpp::PF_Ratchet] = 0.75f; s.lock[7][mpp::PF_Distort] = 0.5f; s.glide[7] = 0.6f;
        s.lock[11][mpp::PF_Cutoff] = -0.5f; s.lock[11][mpp::PF_LzLevel] = 0.8f; s.glide[11] = 0.8f; s.prob[11] = 0.75f;
        s.lock[14][mpp::PF_RpMix] = 1.0f; s.lock[14][mpp::PF_RpCrush] = 0.9f; s.lock[14][mpp::PF_RpRev] = 0.6f;
        s.lock[15][mpp::PF_TzShape] = -0.9f; s.lock[15][mpp::PF_TzMix] = 1.0f;
    }

    // FX / master
    p.apvts.getParameter ("d_fb")->setValueNotifyingHost (p.apvts.getParameter ("d_fb")->convertTo0to1 (0.4f));
    p.apvts.getParameter ("d_glitch")->setValueNotifyingHost (0.2f);
    p.apvts.getParameter ("d_pong")->setValueNotifyingHost (1.0f);
    p.apvts.getParameter ("r_mode")->setValueNotifyingHost (p.apvts.getParameter ("r_mode")->convertTo0to1 (2));
    p.apvts.getParameter ("r_decay")->setValueNotifyingHost (p.apvts.getParameter ("r_decay")->convertTo0to1 (2.5f));
    p.apvts.getParameter ("m_max")->setValueNotifyingHost (0.25f);
    p.apvts.getParameter ("humanize")->setValueNotifyingHost (0.15f);
    p.apvts.getParameter ("bpm")->setValueNotifyingHost (p.apvts.getParameter ("bpm")->convertTo0to1 (126));
    // performance: un po' di decay e distort in più
    p.apvts.getParameter ("pf_envd")->setValueNotifyingHost (p.apvts.getParameter ("pf_envd")->convertTo0to1 (0.15f));
    p.apvts.getParameter ("pf_distort")->setValueNotifyingHost (p.apvts.getParameter ("pf_distort")->convertTo0to1 (0.1f));
    p.apvts.getParameter ("pf_density")->setValueNotifyingHost (p.apvts.getParameter ("pf_density")->convertTo0to1 (-0.1f));
    p.param (3, "pfFlip")->setValueNotifyingHost (1.0f);

    auto kit = ManipoloProcessor::userFolder().getChildFile ("Kits").getChildFile ("Drummatic Demo.mkit");
    const bool ok = p.saveKit (kit);
    std::printf ("kit %s -> %s\n", ok ? "OK" : "FAIL", kit.getFullPathName().toRawUTF8());

    if (argc > 1 && juce::String (argv[1]) == "--standalone")
    {
        juce::MemoryBlock mb; p.getStateInformation (mb);
        juce::PropertiesFile::Options o; o.applicationName = "Drummatic"; o.filenameSuffix = ".settings"; o.folderName = "~/.config";
        juce::PropertiesFile props (o);
        props.setValue ("filterState", mb.toBase64Encoding());
        props.saveIfNeeded();
        std::printf ("stato standalone -> %s\n", props.getFile().getFullPathName().toRawUTF8());
    }
    return ok ? 0 : 1;
}
