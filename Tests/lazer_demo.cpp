// Ascolto dimostrativo dello strato LAZER (0.4):
//  1) giro dei 64 motori (un colpo ciascuno)
//  2) 8 battute di groove con tappeto di fondo, glitch e una performance programmata dal sequencer della Performance
#include "../Source/PluginProcessor.h"
#include <cstdio>
using namespace mp;

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, int n, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0)))
        w->writeFromAudioSampleBuffer (b, 0, n);
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000; const int blk = 512;
    auto outDir = juce::File (argc > 1 ? argv[1] : "demo"); outDir.createDirectory();

    // ---- 1) giro dei motori: voce 1, solo LAZER, un colpo ogni 0,45 s via MIDI (canale 2, KIT MODE) ----
    {
        ManipoloProcessor p; p.setPlayConfigDetails (0, 2, sr, blk); p.prepareToPlay (sr, blk);
        p.setParam (0, "synthOn", 0); p.setParam (0, "lzOn", 1); p.setParam (0, "lzKit", 1); p.setParam (0, "fill", 0);
        p.setParam (0, "lzdecay", 0.4f); p.setParam (0, "lzlevel", -4.0f); p.setParam (0, "rsend", 0.25f);
        for (int v = 1; v < 8; ++v) p.setParam (v, "fill", 0);
        const int hit = (int) (sr * 0.45), total = hit * kNumLzEngines + (int) sr;
        juce::AudioBuffer<float> out (2, total), buf (2, blk); juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += blk)
        {
            const int k = pos / hit;
            if (k < kNumLzEngines && pos % hit < blk) midi.addEvent (juce::MidiMessage::noteOn (2, 36 + k, (juce::uint8) 110), pos % hit);
            p.processBlock (buf, midi); midi.clear();
            const int n = std::min (blk, total - pos);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        }
        writeWav (outDir.getChildFile ("Drummatic-Lazer-64-motori.wav"), out, total, sr);
    }
    // ---- 2) groove con glitch programmati ----
    {
        ManipoloProcessor p; p.setPlayConfigDetails (0, 2, sr, blk); p.prepareToPlay (sr, blk);
        p.apvts.getParameter ("bpm")->setValueNotifyingHost (p.apvts.getParameter ("bpm")->convertTo0to1 (124.0f));
        // voce 1 kick + coda 808, voce 2 snare + REPEAT, voce 3 hat, voce 6 laser con TAPZ, voce 7 tappeto Grain Cloud, voce 8 Static
        p.setParam (0, "tail", 4); p.setParam (0, "tailLvl", 0.45f); p.setParam (0, "tailDec", 0.6f);
        p.setParam (1, "rpOn", 1); p.setParam (1, "rpCount", 3); p.setParam (1, "rpGaps", 0.3f); p.setParam (1, "rpRev", 0.3f); p.setParam (1, "rpCrush", 0.4f);
        p.setParam (5, "synthOn", 0); p.setParam (5, "lzOn", 1); p.setParam (5, "lzEng", 4); p.setParam (5, "fill", 3); p.setParam (5, "rotate", 2);
        p.setParam (5, "tzOn", 1); p.setParam (5, "tzTaps", 10); p.setParam (5, "tzShape", 0.7f); p.setParam (5, "dsend", 0.0f); p.setParam (5, "gain", -8.0f);
        p.setParam (6, "synthOn", 0); p.setParam (6, "lzOn", 1); p.setParam (6, "lzEng", 15); p.setParam (6, "fill", 1); p.setParam (6, "steps", 32);
        p.setParam (6, "lzdecay", 6.0f); p.setParam (6, "lzspread", 0.9f); p.setParam (6, "lzlevel", -10.0f); p.setParam (6, "rsend", 0.6f);
        p.setParam (7, "synthOn", 0); p.setParam (7, "lzOn", 1); p.setParam (7, "lzEng", 60); p.setParam (7, "fill", 1); p.setParam (7, "steps", 32);
        p.setParam (7, "lzdecay", 8.0f); p.setParam (7, "lzlevel", -14.0f);
        // sequencer della Performance: 16 passi da 1/4 (4 battute), glitch sui passi 4, 7, 8, 12, 15
        p.apvts.getParameter ("ps_on")->setValueNotifyingHost (1.0f);
        p.apvts.getParameter ("ps_speed")->setValueNotifyingHost (p.apvts.getParameter ("ps_speed")->convertTo0to1 (2));
        auto& s = p.perfSeq;
        s.lock[3][mpp::PF_RpMix] = 1.0f; s.lock[3][mpp::PF_RpGaps] = 0.4f;
        s.lock[6][mpp::PF_LzEngine] = 0.25f; s.lock[6][mpp::PF_LzTimbre] = 0.6f; s.lock[6][mpp::PF_TzMix] = 0.9f;
        s.lock[7][mpp::PF_Ratchet] = 0.75f; s.lock[7][mpp::PF_Distort] = 0.5f; s.glide[7] = 0.6f;
        s.lock[11][mpp::PF_Cutoff] = -0.5f; s.lock[11][mpp::PF_LzLevel] = 0.8f; s.glide[11] = 0.8f;
        s.lock[14][mpp::PF_RpMix] = 1.0f; s.lock[14][mpp::PF_RpCrush] = 0.9f; s.lock[14][mpp::PF_RpRev] = 0.6f;
        s.lock[15][mpp::PF_TzShape] = -0.9f; s.lock[15][mpp::PF_TzMix] = 1.0f;
        p.dm.setTransportMode (DrumMachine::AlwaysPlay);
        const int total = (int) (sr * 60.0 / 124.0 * 4 * 8);
        juce::AudioBuffer<float> out (2, total), buf (2, blk); juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += blk)
        {
            p.processBlock (buf, midi);
            const int n = std::min (blk, total - pos);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        }
        writeWav (outDir.getChildFile ("Drummatic-Lazer-performance-glitch.wav"), out, total, sr);
    }
    return 0;
}
