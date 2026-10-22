// Silenzio improvviso nel plugin intero: Ableton in play (playhead finto con loop), e intanto random di voce, di tutte
// le voci, della Performance, e parametri girati a caso, come chi suona. Segnala i tratti in cui l'uscita resta a zero
// per più di 3 s e, per ognuno, quale stato della macchina era cambiato.
#include "../Source/PluginProcessor.h"
#include <cstdio>

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        p.setTimeInSamples ((juce::int64) (ppq * 60.0 / bpm * 48000)); return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int minutes = argc > 1 ? std::atoi (argv[1]) : 20;
    juce::Random rnd (argc > 2 ? std::atoi (argv[2]) : 3);
    auto p = std::make_unique<ManipoloProcessor>();
    FakeHost host;
    p->setPlayHead (&host);
    p->setPlayConfigDetails (0, 2, 48000, 1024);
    p->prepareToPlay (48000, 1024);
    juce::AudioBuffer<float> buf (2, 1024); juce::MidiBuffer midi;
    const long long total = (long long) minutes * 60 * 48000;
    long long t = 0, silentFrom = -1, nextAct = 48000; int found = 0;
    juce::String lastActs;
    auto params = p->getParameters();
    {   // un campione su metà delle voci
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("silence_test.wav");
        juce::AudioBuffer<float> s (2, 30000);
        for (int i = 0; i < 30000; ++i) { const float x = std::sin (i * 0.05f) * std::exp (-i / 6000.0f) + (rnd.nextFloat() - 0.5f) * std::exp (-i / 800.0f);
                                          s.setSample (0, i, x * 0.5f); s.setSample (1, i, x * 0.45f); }
        f.deleteFile(); juce::WavAudioFormat wav;
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), 48000, 2, 16, {}, 0)))
            w->writeFromAudioSampleBuffer (s, 0, 30000);
        for (int v = 0; v < 8; v += 2) p->loadSample (v, f);
    }
    while (t < total)
    {
        const int n = 64 + rnd.nextInt (961);
        buf.setSize (2, n, false, false, true);
        p->processBlock (buf, midi); midi.clear();
        float pk = buf.getMagnitude (0, n);
        if (pk < 1e-6f) { if (silentFrom < 0) silentFrom = t; }
        else
        {
            if (silentFrom >= 0 && t - silentFrom > 3 * 48000)
            {
                std::printf ("silenzio %.1f s a %.1f s (dopo: %s)\n", (t - silentFrom) / 48000.0, silentFrom / 48000.0, lastActs.toRawUTF8());
                ++found;
            }
            silentFrom = -1;
        }
        if (silentFrom >= 0 && t - silentFrom > 15 * 48000)
        {
            std::printf ("SILENZIO PERSISTENTE da %.1f s, running=%d mode=%d (ultime azioni: %s)\n", silentFrom / 48000.0,
                         (int) p->dm.running.load(), (int) p->dm.transportMode.load(), lastActs.toRawUTF8());
            // cosa lo fa ripartire?
            p->dm.setTransportMode (mp::DrumMachine::Stopped);
            for (int i = 0; i < 4; ++i) p->processBlock (buf, midi);
            host.playing = false; for (int i = 0; i < 4; ++i) p->processBlock (buf, midi);
            host.playing = true; float pk2 = 0;
            for (int i = 0; i < 400; ++i) { p->processBlock (buf, midi); pk2 = std::max (pk2, buf.getMagnitude (0, n)); }
            std::printf ("  dopo stop/play del DAW: picco %.3f\n", pk2);
            return 2;
        }
        host.ppq += n / 48000.0 * host.bpm / 60.0;
        if (host.ppq >= 32) host.ppq -= 32;
        t += n;
        if (t >= nextAct)
        {
            const int a = rnd.nextInt (10);
            juce::String act;
            if (a < 3) { p->randomizeAll(); act = "RANDOM tutte"; }
            else if (a < 5) { const int v = rnd.nextInt (8); p->randomizeVoice (v); act = "DADO voce " + juce::String (v + 1); }
            else if (a == 5) { p->perfRandomize(); act = "DADO perf"; }
            else if (a == 6)
            {   // MAP: una modulazione a caso
                const int v = rnd.nextInt (8), s = rnd.nextInt (mp::kNumSources), tg = rnd.nextInt (mp::kNumTargets);
                p->dm.vshared[v].depth[s][tg] = rnd.nextFloat() * 2 - 1;
                act = "MAP v" + juce::String (v + 1) + " s" + juce::String (s) + " t" + juce::String (tg);
            }
            else
            {
                act = "param";
                for (int k = 0; k < 6; ++k)
                {
                    auto* q = params[rnd.nextInt (params.size())];
                    q->setValueNotifyingHost (rnd.nextFloat());
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q)) act << " " << rp->paramID;
                }
            }
            lastActs = act + (lastActs.isEmpty() ? "" : " | ") + lastActs.substring (0, 300);
            nextAct = t + 48000 / 2 + rnd.nextInt (48000 * 3);
        }
    }
    std::printf ("%d min: %s\n", minutes, found ? "PROBLEMI" : "ok");
    return found ? 1 : 0;
}
