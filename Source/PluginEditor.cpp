#include "PluginEditor.h"

using namespace ui;

ManipoloEditor::ManipoloEditor (ManipoloProcessor& p)
    : AudioProcessorEditor (p), proc (p), ctx (p), column (ctx), voicePage (ctx), seqPage (ctx), mixerPage (ctx), perfPage (ctx)
{
    setLookAndFeel (&look);
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    ctx.onArmedChanged = [this] { voicePage.armedChanged(); };

    addAndMakeVisible (column);
    column.onSelect = [this] (int v) { selectVoice (v); };
    addChildComponent (voicePage); addChildComponent (seqPage); addChildComponent (mixerPage); addChildComponent (perfPage);
    seqPage.onSelectVoice = [this] (int v) { selectVoice (v); };

    for (auto* b : { &tabVoice, &tabSeq, &tabMix, &tabPerf })
    {
        addAndMakeVisible (*b); b->setRadioGroupId (7); b->setClickingTogglesState (true);
    }
    tabVoice.onClick = [this] { showPage (0); };
    tabSeq.onClick = [this] { showPage (1); };
    tabMix.onClick = [this] { showPage (2); };
    tabPerf.onClick = [this] { showPage (3); };

    addAndMakeVisible (playBtn); addAndMakeVisible (stopBtn);
    playBtn.onClick = [this] { proc.dm.setTransportMode (mp::DrumMachine::AlwaysPlay); };
    stopBtn.onClick = [this] { proc.dm.setTransportMode (mp::DrumMachine::Stopped); };
    playBtn.setTooltip ("Avvia il sequencer (in un DAW segue comunque il trasporto)");
    stopBtn.setTooltip ("Ferma il sequencer e smetti di seguire il DAW. Premi PLAY per riarmare");

    for (auto* b : { &kitPrev, &kitNext, &kitSave, &kitLoad, &rndAll }) addAndMakeVisible (*b);
    addAndMakeVisible (keysBtn);
    keysBtn.setClickingTogglesState (true);
    keysBtn.setToggleState (shortcutsOn(), juce::dontSendNotification);
    keysBtn.setTooltip ("Scorciatoie da tastiera (Q W A S D). Nel DAW sono spente di default per lasciare i tasti al DAW");
    keysBtn.onClick = [this] { proc.apvts.state.setProperty ("kbShortcuts", keysBtn.getToggleState(), nullptr); setWantsKeyboardFocus (keysBtn.getToggleState()); };
    addAndMakeVisible (kitName);
    kitName.setFont (ui::font (13, true)); kitName.setJustificationType (juce::Justification::centred);
    kitName.setColour (juce::Label::backgroundColourId, ui::col::bg);
    kitPrev.onClick = [this] { proc.stepKit (-1); selectVoice (ctx.voice); };
    kitNext.onClick = [this] { proc.stepKit (1); selectVoice (ctx.voice); };
    rndAll.onClick = [this] { proc.randomizeAll(); };
    rndAll.setTooltip ("Randomizza tutte le voci");
    kitSave.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Salva kit", ManipoloProcessor::userFolder().getChildFile ("Kits"), "*.mkit");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        { auto f = fc.getResult(); if (f != juce::File()) proc.saveKit (f.withFileExtension ("mkit")); });
    };
    kitLoad.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Carica kit", ManipoloProcessor::userFolder().getChildFile ("Kits"), "*.mkit");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().existsAsFile()) { proc.loadKit (fc.getResult()); selectVoice (ctx.voice); } });
    };

    addAndMakeVisible (bpm);
    bpm.setSliderStyle (juce::Slider::IncDecButtons);
    bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 24);
    bpm.setColour (juce::Slider::textBoxOutlineColourId, ui::col::line);
    bpmAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("bpm"), bpm, nullptr);
    bpm.setTooltip ("Tempo usato quando nessun DAW fornisce il tempo (standalone)");

    for (auto* k : { &maximize, &eqHigh, &eqMid, &eqLow, &mGain, &rndDepth }) addAndMakeVisible (*k);
    maximize.bind ("m_max", -1, false); eqHigh.bind ("m_high", -1, false); eqMid.bind ("m_mid", -1, false);
    eqLow.bind ("m_low", -1, false); mGain.bind ("m_gain", -1, false); rndDepth.bind ("rnd_depth", -1, false);
    for (auto* k : { &eqHigh, &eqMid, &eqLow }) k->setBipolar (true);
    addAndMakeVisible (clip); clip.bind ("m_clip", false);

    setWantsKeyboardFocus (shortcutsOn());
    setResizable (true, true);
    setResizeLimits (1200, 720, 1900, 1150);
    setSize (1280, 780);
    showPage (0);
    tabVoice.setToggleState (true, juce::dontSendNotification);
    startTimerHz (15);
}

ManipoloEditor::~ManipoloEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ManipoloEditor::selectVoice (int v)
{
    ctx.voice = v;
    voicePage.rebind(); seqPage.rebind(); mixerPage.rebind();
    column.repaint(); mixerPage.repaint();
}

void ManipoloEditor::showPage (int p)
{
    page = p;
    voicePage.setVisible (p == 0); seqPage.setVisible (p == 1); mixerPage.setVisible (p == 2); perfPage.setVisible (p == 3);
    (p == 0 ? tabVoice : p == 1 ? tabSeq : p == 2 ? tabMix : tabPerf).setToggleState (true, juce::dontSendNotification);
}

void ManipoloEditor::timerCallback()
{
    const bool running = proc.dm.running.load();
    const int mode = proc.dm.transportMode.load();
    // PLAY lampeggia quando segue il DAW ma il DAW è fermo
    const bool blink = mode == mp::DrumMachine::FollowDaw && ! running && (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
    playBtn.setToggleState (running || blink, juce::dontSendNotification);
    stopBtn.setToggleState (mode == mp::DrumMachine::Stopped, juce::dontSendNotification);
    bpm.setVisible (! proc.hostDrivesTransport.load());
    kitName.setText (proc.currentKit.existsAsFile() ? proc.currentKit.getFileNameWithoutExtension() : juce::String ("Kit senza nome"), juce::dontSendNotification);
}

void ManipoloEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::col::bg);
    g.setColour (ui::col::panel); g.fillRect (0, 0, getWidth(), 44);
    g.setColour (ui::col::line); g.drawHorizontalLine (44, 0, (float) getWidth());
    g.setColour (ui::col::accent); g.setFont (ui::font (22, true));
    g.drawText ("MANIPOLO", 14, 0, 150, 44, juce::Justification::centredLeft);
    g.setColour (ui::col::dim); g.setFont (ui::font (10));
    g.drawText ("drum synth", 16, 28, 120, 14, juce::Justification::centredLeft);
    // master
    auto m = getLocalBounds().removeFromRight (120).withTrimmedTop (50).reduced (4);
    ui::drawPanel (g, m, "MASTER");
}

void ManipoloEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (44).reduced (6, 7);
    header.removeFromLeft (150);
    playBtn.setBounds (header.removeFromLeft (56)); header.removeFromLeft (3);
    stopBtn.setBounds (header.removeFromLeft (56)); header.removeFromLeft (10);
    bpm.setBounds (header.removeFromLeft (110)); header.removeFromLeft (10);
    for (auto* b : { &tabVoice, &tabSeq, &tabMix, &tabPerf }) { b->setBounds (header.removeFromLeft (80)); header.removeFromLeft (3); }
    header.removeFromLeft (10);
    rndAll.setBounds (header.removeFromRight (72)); header.removeFromRight (6);
    keysBtn.setBounds (header.removeFromRight (52)); header.removeFromRight (6);
    kitSave.setBounds (header.removeFromRight (84)); header.removeFromRight (3);
    kitLoad.setBounds (header.removeFromRight (90)); header.removeFromRight (6);
    kitNext.setBounds (header.removeFromRight (26)); header.removeFromRight (2);
    kitName.setBounds (header.removeFromRight (std::max (80, std::min (220, header.getWidth() - 30))));
    header.removeFromRight (2);
    kitPrev.setBounds (header.removeFromRight (26));

    r.removeFromTop (6);
    auto master = r.removeFromRight (120).reduced (4);
    {
        auto in = master.reduced (6).withTrimmedTop (26);
        maximize.setBounds (in.removeFromTop (84));
        in.removeFromTop (8);
        eqHigh.setBounds (in.removeFromTop (78)); in.removeFromTop (4); eqMid.setBounds (in.removeFromTop (78)); in.removeFromTop (4); eqLow.setBounds (in.removeFromTop (78));
        in.removeFromTop (6);
        clip.setBounds (in.removeFromTop (24)); in.removeFromTop (6);
        mGain.setBounds (in.removeFromTop (84));
        rndDepth.setBounds (in.removeFromBottom (78));
    }
    column.setBounds (r.removeFromLeft (150).reduced (4, 0));
    for (auto* p : { (juce::Component*) &voicePage, (juce::Component*) &seqPage, (juce::Component*) &mixerPage, (juce::Component*) &perfPage }) p->setBounds (r);
}

bool ManipoloEditor::keyPressed (const juce::KeyPress& k)
{
    // dentro un DAW i tasti vanno al DAW (spazio, tastiera MIDI del computer…), salvo attivazione esplicita
    if (! shortcutsOn()) return false;
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == 'q') { proc.dm.auditionVoice (ctx.voice); return true; }
    if (c == 'w') { selectVoice ((ctx.voice + mp::kNumVoices - 1) % mp::kNumVoices); return true; }
    if (c == 's') { selectVoice ((ctx.voice + 1) % mp::kNumVoices); return true; }
    if (c == 'a') { showPage ((page + 3) % 4); return true; }
    if (c == 'd') { showPage ((page + 1) % 4); return true; }
    if (k == juce::KeyPress::spaceKey && proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        proc.dm.setTransportMode (proc.dm.running.load() ? mp::DrumMachine::Stopped : mp::DrumMachine::AlwaysPlay);
        return true;
    }
    return false;
}
