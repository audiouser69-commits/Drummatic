#pragma once
#include "Widgets.h"

namespace ui
{
// Colonna ENABLE / FLIP per le 8 voci
class PerfToggles : public juce::Component
{
public:
    explicit PerfToggles (Context& c) : ctx (c)
    {
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            auto* e = enables.add (new juce::ToggleButton()); auto* f = flips.add (new juce::ToggleButton());
            addAndMakeVisible (e); addAndMakeVisible (f);
            e->setColour (juce::ToggleButton::tickColourId, col::accent); f->setColour (juce::ToggleButton::tickColourId, col::accent2);
            e->setColour (juce::ToggleButton::tickDisabledColourId, col::dim); f->setColour (juce::ToggleButton::tickDisabledColourId, col::dim);
            atts.add (new juce::ButtonParameterAttachment (*ctx.proc.param (v, "pfEnable"), *e, nullptr));
            atts.add (new juce::ButtonParameterAttachment (*ctx.proc.param (v, "pfFlip"), *f, nullptr));
            e->setTooltip ("Voce " + juce::String (v + 1) + ": risponde alla pagina Performance");
            f->setTooltip ("Voce " + juce::String (v + 1) + ": effetto invertito (FLIP)");
        }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::dim); g.setFont (font (10.5f, true));
        g.drawText ("ENABLE", 26, 0, 50, 16, juce::Justification::centred);
        g.drawText ("FLIP", 76, 0, 50, 16, juce::Justification::centred);
        const int h = (getHeight() - 18) / mp::kNumVoices;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            g.setColour (col::accent); g.setFont (font (15, true));
            g.drawText (juce::String (v + 1), 0, 18 + v * h, 24, h, juce::Justification::centred);
        }
    }
    void resized() override
    {
        const int h = (getHeight() - 18) / mp::kNumVoices;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            enables[v]->setBounds (36, 18 + v * h + h / 2 - 12, 30, 24);
            flips[v]->setBounds (86, 18 + v * h + h / 2 - 12, 30, 24);
        }
    }
private:
    Context& ctx;
    juce::OwnedArray<juce::ToggleButton> enables, flips;
    juce::OwnedArray<juce::ButtonParameterAttachment> atts;
};

class PerfPage : public juce::Component
{
public:
    explicit PerfPage (Context& c) : ctx (c), toggles (c), enabled (c, "ENABLED")
    {
        addAndMakeVisible (toggles);
        for (int i = 0; i < mpp::kNumPerf; ++i)
        {
            auto* k = knobs.add (new ParamKnob (ctx, mpp::perfLabel (i)));
            k->bind (mpp::perfId (i), -1, false);
            k->setBipolar (true);
            k->getKnob().lockFlag = &ctx.proc.perfLock[i];
            k->getKnob().setTooltip (k->getKnob().getTooltip() + "  (tasto destro: esclude dal RANDOMIZE)");
            addAndMakeVisible (k);
        }
        addAndMakeVisible (depth); depth.bind ("pf_depth", -1, false); depth.setBipolar (true);
        addAndMakeVisible (rndDepth); rndDepth.bind ("pf_rnd", -1, false);
        addAndMakeVisible (enabled); enabled.bind ("pf_enabled", false);
        for (auto* b : { &rndBtn, &commitBtn, &resetBtn }) addAndMakeVisible (*b);
        rndBtn.onClick = [this] { ctx.proc.perfRandomize(); };
        commitBtn.onClick = [this] { ctx.proc.perfCommit(); };
        resetBtn.onClick = [this] { ctx.proc.perfReset(); };
        rndBtn.setTooltip ("Randomizza tutti i controlli Performance non bloccati");
        commitBtn.setTooltip ("Scrive il suono attuale nelle voci e azzera la Performance");
        resetBtn.setTooltip ("Riporta tutti i controlli Performance a 0");
    }

    void paint (juce::Graphics& g) override { for (auto& p : panels) drawPanel (g, p.first, p.second); }

    void resized() override
    {
        panels.clear();
        auto r = getLocalBounds().reduced (6);
        auto left = r.removeFromLeft (130); r.removeFromLeft (6);
        panels.push_back ({ left, "" });
        toggles.setBounds (left.reduced (4, 8));
        auto global = r.removeFromRight (200); r.removeFromRight (6);
        panels.push_back ({ global, "GLOBAL" });
        {
            auto in = global.reduced (8).withTrimmedTop (24);
            depth.setBounds (in.removeFromTop (150)); in.removeFromTop (10);
            auto row = in.removeFromTop (30);
            const int w = row.getWidth() / 3;
            rndBtn.setBounds (row.removeFromLeft (w).reduced (2)); commitBtn.setBounds (row.removeFromLeft (w).reduced (2)); resetBtn.setBounds (row.reduced (2));
            in.removeFromTop (10);
            rndDepth.setBounds (in.removeFromTop (90));
            in.removeFromTop (10);
            enabled.setBounds (in.removeFromTop (28).reduced (20, 0));
        }
        // sezioni
        const int rowH = (r.getHeight() - 5 * 6) / 6;
        auto section = [&] (const char* title, int from, int to)
        {
            auto a = r.removeFromTop (rowH); r.removeFromTop (6);
            panels.push_back ({ a, title });
            auto in = a.reduced (6).withTrimmedTop (22);
            const int w = in.getWidth() / 6;
            for (int i = from; i <= to; ++i) knobs[i]->setBounds (in.removeFromLeft (w));
        };
        section ("ENV", mpp::PF_EnvA, mpp::PF_EnvC);
        section ("SYNTH", mpp::PF_SynPitch, mpp::PF_Syn4);
        section ("SAMPLER", mpp::PF_SmpPitch, mpp::PF_Smp4);
        section ("MODULATION", mpp::PF_Macro1, mpp::PF_ModAmp);
        section ("TONE", mpp::PF_Distort, mpp::PF_Res);
        section ("SEQUENCER", mpp::PF_Steps, mpp::PF_Ratchet);
    }

private:
    Context& ctx;
    PerfToggles toggles;
    juce::OwnedArray<ParamKnob> knobs;
    ParamKnob depth { ctx, "DEPTH" }, rndDepth { ctx, "RANDOMIZE DEPTH" };
    ParamToggle enabled;
    juce::TextButton rndBtn { "DADO" }, commitBtn { "COMMIT" }, resetBtn { "RESET" };
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
};
} // namespace ui
