// Schermate dell'interfaccia (VOICE, LAZER, SEQ, MIX, PERFORM) salvate come PNG in shots/
#include "../Source/PluginEditor.h"
#include <cstdio>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto p = std::make_unique<ManipoloProcessor>();
    p->setPlayConfigDetails (0, 2, 48000, 512);
    p->prepareToPlay (48000, 512);
    // un po' di contenuto: voce 1 con LAZER, sequenza Performance di glitch
    p->setParam (0, "lzOn", 1); p->setParam (0, "lzEng", 13); p->setParam (0, "tzOn", 1); p->setParam (0, "rpOn", 1); p->setParam (0, "tail", 3);
    p->apvts.getParameter ("ps_on")->setValueNotifyingHost (1.0f);
    p->perfSeqRandomGlitch();
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    ed->setSize (1280, 780);
    auto out = juce::File (argc > 1 ? argv[1] : "shots");
    out.createDirectory();
    auto* me = dynamic_cast<ManipoloEditor*> (ed.get());
    for (int page = 0; page < 5; ++page)
    {
        me->showPageForTest (page);
        ed->resized();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        juce::PNGImageFormat png; auto f = out.getChildFile ("page" + juce::String (page) + ".png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
