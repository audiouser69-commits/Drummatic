// Schermate dell'interfaccia (VOICE, LAZER, SEQ, MIX, PERFORM) salvate come PNG in shots/
#include "../Source/PluginEditor.h"
#include <cstdio>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto p = std::make_unique<ManipoloProcessor>();
    p->setPlayConfigDetails (0, 2, 48000, 512);
    p->prepareToPlay (48000, 512);
    // kit da mostrare (es. "Drummatic Demo.mkit"), altrimenti un po' di contenuto d'esempio
    if (argc > 2 && juce::File (argv[2]).existsAsFile()) p->loadKit (juce::File (argv[2]));
    else
    {
        p->setParam (0, "lzOn", 1); p->setParam (0, "lzEng", 13); p->setParam (0, "tzOn", 1); p->setParam (0, "rpOn", 1); p->setParam (0, "tail", 3);
        p->apvts.getParameter ("ps_on")->setValueNotifyingHost (1.0f);
        p->perfSeqRandomGlitch();
    }
    // qualche battuta di riproduzione: LED, passo del sequencer Performance, valori letti dal motore
    p->dm.setTransportMode (mp::DrumMachine::AlwaysPlay);
    { juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; for (int i = 0; i < 300; ++i) p->processBlock (b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    ed->setSize (1280, 780);
    auto out = juce::File (argc > 1 ? argv[1] : "shots");
    out.createDirectory();
    auto* me = dynamic_cast<ManipoloEditor*> (ed.get());
    // voce mostrata per pagina: VOICE la 7 (Scrapyard + Granular), LAZER la 6, le altre la 1
    const int voiceFor[5] = { 6, 5, 0, 0, 0 };
    for (int page = 0; page < 5; ++page)
    {
        me->selectVoiceForTest (voiceFor[page]);
        me->showPageForTest (page);
        ed->resized();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        juce::PNGImageFormat png; auto f = out.getChildFile ("page" + juce::String (page) + ".png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    // schermate in più per il manuale: MAP su RANDOM, SEQ voce 8 lane PITCH, vista ALL
    auto snap = [&] (const char* name)
    {
        ed->resized();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        juce::PNGImageFormat png; auto f = out.getChildFile (name); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    };
    me->selectVoiceForTest (6); me->showPageForTest (0); me->armForTest (mp::S_Random); snap ("map.png"); me->armForTest (-1);
    me->selectVoiceForTest (7); me->showPageForTest (2); me->seqForTest (mp::L_Pitch, false); snap ("seq.png");
    me->seqForTest (mp::L_Pitch, true); snap ("all.png");
    return 0;
}
