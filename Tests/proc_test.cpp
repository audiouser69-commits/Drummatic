// Test del processore JUCE: rendering, stato, preset
#include "../Source/PluginProcessor.h"
#include <cstdio>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto p = std::make_unique<ManipoloProcessor>();
    p->setPlayConfigDetails (0, 2, 48000, 512);
    p->prepareToPlay (48000, 512);
    p->dm.setTransportMode (mp::DrumMachine::AlwaysPlay);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    double rms = 0; int nans = 0;
    for (int b = 0; b < 400; ++b)
    {
        if (b == 10) midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);   // ch1 E -> voce 5
        if (b == 20) midi.addEvent (juce::MidiMessage::noteOn (3, 67, (juce::uint8) 100), 0);   // ch3 -> voce 2, +7st
        p->processBlock (buf, midi); midi.clear();
        for (int i = 0; i < 512; ++i) { float x = buf.getSample (0, i); if (! std::isfinite (x)) ++nans; rms += x * x; }
    }
    rms = std::sqrt (rms / (400 * 512));
    std::printf ("render rms=%.4f nans=%d\n", rms, nans);

    // modifica stato e roundtrip
    p->dm.vshared[2].depth[mp::S_Lfo1][mp::T_Cutoff] = 0.42f;
    p->dm.vshared[2].locked[mp::T_Pitch] = true;
    p->dm.sshared[3].manual[5] = true;
    p->dm.sshared[3].lane[mp::L_Pitch][2] = 7;
    p->setParam (4, "engine", 4);
    juce::MemoryBlock mb; p->getStateInformation (mb);
    auto q = std::make_unique<ManipoloProcessor>();
    q->setStateInformation (mb.getData(), (int) mb.getSize());
    const bool ok1 = std::abs (q->dm.vshared[2].depth[mp::S_Lfo1][mp::T_Cutoff].load() - 0.42f) < 1e-3f
                  && q->dm.vshared[2].locked[mp::T_Pitch].load() && q->dm.sshared[3].manual[5].load()
                  && q->dm.sshared[3].lane[mp::L_Pitch][2].load() == 7
                  && juce::roundToInt (q->param (4, "engine")->convertFrom0to1 (q->param (4, "engine")->getValue())) == 4;
    std::printf ("state roundtrip %s (state %d bytes)\n", ok1 ? "OK" : "FAIL", (int) mb.getSize());

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto vf = tmp.getChildFile ("t.mvoice");
    p->saveVoicePreset (2, vf);
    q->resetVoice (0);
    q->loadVoicePreset (0, vf);
    const bool ok2 = std::abs (q->dm.vshared[0].depth[mp::S_Lfo1][mp::T_Cutoff].load() - 0.42f) < 1e-3f;
    std::printf ("voice preset %s\n", ok2 ? "OK" : "FAIL");
    p->randomizeVoice (1); p->randomizeAll();
    for (int b = 0; b < 100; ++b) { p->processBlock (buf, midi); for (int i = 0; i < 512; ++i) if (! std::isfinite (buf.getSample (0, i))) ++nans; }
    std::printf ("after randomize nans=%d\n", nans);
    // Performance: PITCH synth +50% -> COMMIT -> +12 st sulla voce 1
    p->perfReset();
    p->apvts.getParameter ("pf_spitch")->setValueNotifyingHost (p->apvts.getParameter ("pf_spitch")->convertTo0to1 (0.5f));
    p->setParam (0, "pitch", 0.0f);
    p->processBlock (buf, midi);
    p->perfCommit();
    const float newPitch = p->param (0, "pitch")->convertFrom0to1 (p->param (0, "pitch")->getValue());
    const float perfAfter = p->apvts.getRawParameterValue ("pf_spitch")->load();
    const bool ok3 = std::abs (newPitch - 12.0f) < 0.6f && std::abs (perfAfter) < 1e-4f;
    std::printf ("perf commit pitch=%.2f perf=%.2f %s\n", newPitch, perfAfter, ok3 ? "OK" : "FAIL");
    // uscite multiple: abilita tutti i bus, voce 1 su Out 3-4
    auto layout = p->getBusesLayout();
    for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference (i) = juce::AudioChannelSet::stereo();
    const bool lay = p->setBusesLayout (layout);
    p->prepareToPlay (48000, 512);
    p->setParam (0, "route", 1);
    juce::AudioBuffer<float> big (16, 512); double auxE = 0;
    for (int b = 0; b < 200; ++b) { p->processBlock (big, midi); for (int i = 0; i < 512; ++i) auxE += std::abs (big.getSample (2, i)); }
    const bool ok4 = lay && auxE > 1.0;
    std::printf ("multiout layout=%d auxEnergy=%.1f %s\n", (int) lay, auxE, ok4 ? "OK" : "FAIL");
    return (rms > 0.01 && ok3 && ok4 && nans == 0 && ok1 && ok2) ? 0 : 1;
}
