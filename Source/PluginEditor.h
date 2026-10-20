#pragma once
#include "PluginProcessor.h"
#include "UI/VoicePage.h"
#include "UI/SeqPage.h"
#include "UI/MixerPage.h"
#include "UI/PerfPage.h"

// Colonna di selezione voci con LED di attività e M/S
class VoiceColumn : public juce::Component, private juce::Timer
{
public:
    VoiceColumn (ui::Context& c) : ctx (c) { startTimerHz (30); }
    std::function<void (int)> onSelect;
    void paint (juce::Graphics& g) override
    {
        const int h = getHeight() / mp::kNumVoices;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            auto r = juce::Rectangle<int> (0, v * h, getWidth(), h).reduced (2);
            const bool sel = v == ctx.voice;
            g.setColour (sel ? ui::col::accent.withAlpha (0.18f) : ui::col::panel); g.fillRoundedRectangle (r.toFloat(), 4);
            if (sel) { g.setColour (ui::col::accent); g.fillRect (r.getX(), r.getY() + 4, 3, r.getHeight() - 8); }
            g.setColour (sel ? ui::col::accent : ui::col::text); g.setFont (ui::font (20, true));
            g.drawText (juce::String (v + 1), r.getX() + 6, r.getY() + 4, 24, 24, juce::Justification::centredLeft);
            // nome motore attivo
            auto* eng = ctx.proc.param (v, "engine");
            const bool syn = ctx.proc.param (v, "synthOn")->getValue() > 0.5f, smp = ctx.proc.param (v, "sampOn")->getValue() > 0.5f;
            juce::String name = syn ? eng->getCurrentValueAsText() : juce::String();
            if (smp) name += syn ? " + S" : "Sampler";
            g.setColour (ui::col::dim); g.setFont (ui::font (10.5f));
            g.drawText (name, r.getX() + 6, r.getBottom() - 18, r.getWidth() - 10, 14, juce::Justification::centredLeft);
            // LED
            const float lvl = juce::jlimit (0.0f, 1.0f, ctx.proc.dm.vshared[v].level.load() * 2.0f);
            g.setColour (ui::col::accent2.withAlpha (0.15f + 0.85f * lvl)); g.fillEllipse ((float) r.getRight() - 16, (float) r.getY() + 9, 9, 9);
            // M/S
            const bool m = ctx.proc.dm.mute[v].load(), s = ctx.proc.dm.solo[v].load();
            auto mr = msRect (v, 0), sr = msRect (v, 1);
            g.setColour (m ? ui::col::warn : ui::col::panel2); g.fillRoundedRectangle (mr.toFloat(), 2);
            g.setColour (s ? ui::col::accent2 : ui::col::panel2); g.fillRoundedRectangle (sr.toFloat(), 2);
            g.setColour (m ? juce::Colours::black : ui::col::dim); g.setFont (ui::font (10, true)); g.drawText ("M", mr, juce::Justification::centred);
            g.setColour (s ? juce::Colours::black : ui::col::dim); g.drawText ("S", sr, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = getHeight() / mp::kNumVoices;
        const int v = juce::jlimit (0, mp::kNumVoices - 1, e.y / h);
        if (msRect (v, 0).contains (e.getPosition())) { ctx.proc.dm.mute[v] = ! ctx.proc.dm.mute[v].load(); repaint(); return; }
        if (msRect (v, 1).contains (e.getPosition())) { ctx.proc.dm.solo[v] = ! ctx.proc.dm.solo[v].load(); repaint(); return; }
        if (onSelect) onSelect (v);
        if (e.mods.isShiftDown() || e.getNumberOfClicks() > 1) ctx.proc.dm.auditionVoice (v);
    }
private:
    ui::Context& ctx;
    juce::Rectangle<int> msRect (int v, int which)
    {
        const int h = getHeight() / mp::kNumVoices;
        return { getWidth() - 44 + which * 20, v * h + h / 2 - 7, 18, 14 };
    }
    void timerCallback() override { repaint(); }
};

class ManipoloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ManipoloEditor (ManipoloProcessor&);
    ~ManipoloEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    ManipoloProcessor& proc;
    ui::Look look;
    ui::Context ctx;
    VoiceColumn column;
    ui::VoicePage voicePage;
    ui::SeqPage seqPage;
    ui::MixerPage mixerPage;
    ui::PerfPage perfPage;
    int page = 0;
    juce::TextButton tabVoice { "VOICE" }, tabSeq { "SEQ" }, tabMix { "MIX / FX" }, tabPerf { "PERFORM" };
    juce::TextButton playBtn { "PLAY" }, stopBtn { "STOP" };
    juce::TextButton keysBtn { "TASTI" };
    bool shortcutsOn() const
    {
        return (bool) proc.apvts.state.getProperty ("kbShortcuts", proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    }
    juce::TextButton kitPrev { "<" }, kitNext { ">" }, kitSave { "SALVA KIT" }, kitLoad { "CARICA KIT" }, rndAll { "RANDOM" };
    juce::Label kitName;
    juce::Slider bpm; std::unique_ptr<juce::SliderParameterAttachment> bpmAtt;
    ui::ParamKnob maximize { ctx, "MAXIMIZE" }, eqHigh { ctx, "HIGH" }, eqMid { ctx, "MID" }, eqLow { ctx, "LOW" }, mGain { ctx, "GAIN" }, rndDepth { ctx, "RND DEPTH" };
    ui::ParamCombo clip { ctx };
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    void selectVoice (int v);
    void showPage (int p);
    void timerCallback() override;
};
