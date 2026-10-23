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

// Sequencer della Performance: ogni passo può bloccare i controlli Performance su un valore ("scena" programmata)
class PerfSeqStrip : public juce::Component, private juce::Timer
{
public:
    explicit PerfSeqStrip (Context& c) : ctx (c), onBtn (c, "SEQ ON"), speed (c)
    {
        addAndMakeVisible (onBtn); onBtn.bind ("ps_on", false);
        onBtn.setTooltip ("Accende il sequencer della Performance (segue il trasporto)");
        addAndMakeVisible (speed); speed.bind ("ps_speed", false);
        speed.setTooltip ("Durata di un passo");
        addAndMakeVisible (len);
        len.setSliderStyle (juce::Slider::IncDecButtons); len.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 22);
        len.setColour (juce::Slider::textBoxOutlineColourId, col::line);
        lenAtt = std::make_unique<juce::SliderParameterAttachment> (*ctx.proc.apvts.getParameter ("ps_len"), len, nullptr);
        len.setTooltip ("Numero di passi");
        for (auto* b : { &recBtn, &capBtn, &clrBtn, &rndBtn, &wipeBtn }) addAndMakeVisible (*b);
        recBtn.setClickingTogglesState (true);
        recBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8423a));
        recBtn.onClick = [this] { ctx.proc.perfSeq.rec = recBtn.getToggleState(); };
        recBtn.setTooltip ("REC: con il sequencer in riproduzione, i movimenti delle manopole Performance vengono scritti nel passo che suona");
        capBtn.onClick = [this] { ctx.proc.perfSeqCapture (sel); repaint(); };
        capBtn.setTooltip ("Scrive nel passo selezionato i valori attuali delle manopole Performance (quelle diverse da 0)");
        clrBtn.onClick = [this] { ctx.proc.perfSeq.clearStep (sel); syncStepControls(); repaint(); };
        clrBtn.setTooltip ("Cancella i blocchi del passo selezionato (tasto destro su un passo fa lo stesso)");
        rndBtn.onClick = [this] { ctx.proc.perfSeqRandomGlitch(); repaint(); };
        rndBtn.setTooltip ("Genera una performance di glitch casuale (LAZER, TAPZ, REPEAT, distorsione, filtro, ratchet). Rispetta i lucchetti");
        wipeBtn.onClick = [this] { ctx.proc.perfSeq.clearAll(); syncStepControls(); repaint(); };
        wipeBtn.setTooltip ("Svuota tutta la sequenza");
        addAndMakeVisible (view);
        view.addItem ("Vista: blocchi per passo", 1);
        for (int i = 0; i < mpp::kNumPerf; ++i) view.addItem (juce::String ("Disegna: ") + perfSection (i) + " " + mpp::perfLabel (i), i + 2);
        view.setSelectedId (1, juce::dontSendNotification);
        view.onChange = [this] { repaint(); };
        view.setTooltip ("Scegli un controllo per disegnarne i valori passo per passo (trascina in verticale, Alt+clic toglie il blocco)");
        for (auto* sl : { &glide, &prob })
        {
            addAndMakeVisible (*sl);
            sl->setSliderStyle (juce::Slider::LinearHorizontal); sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
            sl->setRange (0, 100, 1); sl->setColour (juce::Slider::textBoxOutlineColourId, col::line);
        }
        glide.onValueChange = [this] { ctx.proc.perfSeq.glide[sel] = (float) glide.getValue() / 100.0f; repaint(); };
        prob.onValueChange = [this] { ctx.proc.perfSeq.prob[sel] = (float) prob.getValue() / 100.0f; repaint(); };
        glide.setTooltip ("GLIDE del passo: scivola dal passo precedente per questa parte del passo");
        prob.setTooltip ("PROB del passo: probabilità che il passo scatti (altrimenti resta la scena precedente)");
        syncStepControls();
        startTimerHz (20);
    }
    static const char* perfSection (int i)
    {
        using namespace mpp;
        if (i <= PF_EnvC) return "ENV"; if (i <= PF_Syn4) return "SYN"; if (i <= PF_Smp4) return "SMP"; if (i <= PF_ModAmp) return "MOD";
        if (i <= PF_Res) return "TONE"; if (i <= PF_Ratchet) return "SEQ"; if (i <= PF_LzDecay) return "LAZER"; return "BRK";
    }
    void paint (juce::Graphics& g) override
    {
        drawPanel (g, getLocalBounds(), "SEQUENCER PERFORMANCE");
        auto& ps = ctx.proc.perfSeq;
        const int n = steps(), play = ps.playStep.load(), viewI = view.getSelectedId() - 2;
        g.setColour (col::dim); g.setFont (font (10, true));
        g.drawText ("GLIDE", glide.getX() - 40, glide.getY(), 38, glide.getHeight(), juce::Justification::centredRight);
        g.drawText ("PROB", prob.getX() - 36, prob.getY(), 34, prob.getHeight(), juce::Justification::centredRight);
        g.drawText ("PASSI", len.getX() - 40, len.getY(), 38, len.getHeight(), juce::Justification::centredRight);
        for (int st = 0; st < n; ++st)
        {
            auto r = cell (st).toFloat();
            g.setColour (st % 4 == 0 ? col::panel2.brighter (0.06f) : col::panel2); g.fillRoundedRectangle (r, 2);
            const int cnt = ps.lockCount (st);
            if (viewI >= 0)
            {
                // valore bipolare del controllo scelto
                g.setColour (col::line); g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 1, r.getRight() - 1);
                if (ps.hasLock (st, viewI))
                {
                    const float v = ps.lock[st][viewI].load();
                    const float y0 = r.getCentreY(), y1 = r.getCentreY() - v * (r.getHeight() * 0.45f);
                    g.setColour (col::accent); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX() + 2, std::min (y0, y1), r.getRight() - 2, std::max (y0, y1) + 1));
                }
            }
            else if (cnt > 0)
            {
                const float h = std::min (1.0f, cnt / 6.0f) * (r.getHeight() - 6);
                g.setColour (col::accent.withAlpha (0.85f)); g.fillRect (r.getX() + 3, r.getBottom() - 3 - h, r.getWidth() - 6, h);
                g.setColour (juce::Colours::black); g.setFont (font (9, true));
                g.drawText (juce::String (cnt), r.toNearestInt(), juce::Justification::centredTop);
            }
            if (ps.glide[st].load() > 0)
            {
                g.setColour (col::accent2); juce::Path p; p.startNewSubPath (r.getX() + 2, r.getBottom() - 2);
                p.lineTo (r.getX() + 2 + (r.getWidth() - 4) * ps.glide[st].load(), r.getY() + 3); g.strokePath (p, juce::PathStrokeType (1.2f));
            }
            if (ps.prob[st].load() < 0.999f)
            {
                g.setColour (col::warn); g.setFont (font (8.5f, true));
                g.drawText (juce::String (juce::roundToInt (ps.prob[st].load() * 100)), r.toNearestInt().reduced (1), juce::Justification::bottomRight);
            }
            if (st == play) { g.setColour (col::accent2); g.drawRoundedRectangle (r.reduced (0.5f), 2, 2.0f); }
            if (st == sel) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r.reduced (1.5f), 2, 1.2f); }
        }
    }
    void resized() override
    {
        auto in = getLocalBounds().reduced (8); in.removeFromTop (20);
        auto top = in.removeFromTop (24);
        onBtn.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4);
        recBtn.setBounds (top.removeFromLeft (46)); top.removeFromLeft (44);
        len.setBounds (top.removeFromLeft (88)); top.removeFromLeft (4);
        speed.setBounds (top.removeFromLeft (70)); top.removeFromLeft (8);
        capBtn.setBounds (top.removeFromLeft (72)); top.removeFromLeft (3);
        clrBtn.setBounds (top.removeFromLeft (72)); top.removeFromLeft (3);
        rndBtn.setBounds (top.removeFromLeft (96)); top.removeFromLeft (3);
        wipeBtn.setBounds (top.removeFromLeft (64));
        in.removeFromTop (6);
        auto bottom = in.removeFromBottom (22);
        view.setBounds (bottom.removeFromLeft (230)); bottom.removeFromLeft (48);
        glide.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 24));
        bottom.removeFromLeft (44); prob.setBounds (bottom);
        in.removeFromBottom (6);
        grid = in;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int st = stepAt (e.getPosition());
        if (st < 0) return;
        if (e.mods.isPopupMenu()) { ctx.proc.perfSeq.clearStep (st); repaint(); return; }
        sel = st; syncStepControls();
        const int viewI = view.getSelectedId() - 2;
        if (viewI >= 0)
        {
            if (e.mods.isAltDown()) ctx.proc.perfSeq.lock[st][viewI] = std::numeric_limits<float>::quiet_NaN();
            else drawValue (e.getPosition());
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (view.getSelectedId() >= 2 && ! e.mods.isPopupMenu() && ! e.mods.isAltDown()) { drawValue (e.getPosition()); repaint(); }
    }

private:
    Context& ctx;
    ParamToggle onBtn; ParamCombo speed;
    juce::Slider len, glide, prob; std::unique_ptr<juce::SliderParameterAttachment> lenAtt;
    juce::TextButton recBtn { "REC" }, capBtn { "CATTURA" }, clrBtn { "CANCELLA" }, rndBtn { "DADO GLITCH" }, wipeBtn { "SVUOTA" };
    juce::ComboBox view;
    juce::Rectangle<int> grid;
    int sel = 0, lastPlay = -2;

    int steps() const { return juce::jlimit (1, mpp::kPerfSeqSteps, juce::roundToInt (ctx.proc.apvts.getRawParameterValue ("ps_len")->load())); }
    juce::Rectangle<int> cell (int st) const
    {
        const int n = steps(); const int perRow = n > 32 ? 32 : n; const int rows = n > 32 ? 2 : 1;
        const int w = grid.getWidth() / std::max (1, perRow), h = grid.getHeight() / rows;
        const int row = st / perRow, colI = st % perRow;
        return { grid.getX() + colI * w + 1, grid.getY() + row * h + 1, w - 2, h - 2 };
    }
    int stepAt (juce::Point<int> p) const { for (int st = 0; st < steps(); ++st) if (cell (st).contains (p)) return st; return -1; }
    void drawValue (juce::Point<int> p)
    {
        const int n = steps(), perRow = n > 32 ? 32 : n;
        const int row = (n > 32 && p.y >= grid.getCentreY()) ? 1 : 0;
        const int colI = juce::jlimit (0, perRow - 1, (p.x - grid.getX()) / std::max (1, grid.getWidth() / perRow));
        const int st = row * perRow + colI;
        const int viewI = view.getSelectedId() - 2;
        if (st >= n) return;
        if (st < 0 || viewI < 0) return;
        auto r = cell (st);
        ctx.proc.perfSeq.lock[st][viewI] = juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - (float) p.y) / (r.getHeight() * 0.45f));
    }
    void syncStepControls()
    {
        glide.setValue (ctx.proc.perfSeq.glide[sel].load() * 100.0f, juce::dontSendNotification);
        prob.setValue (ctx.proc.perfSeq.prob[sel].load() * 100.0f, juce::dontSendNotification);
    }
    void timerCallback() override
    {
        recBtn.setToggleState (ctx.proc.perfSeq.rec.load(), juce::dontSendNotification);
        const int p = ctx.proc.perfSeq.playStep.load();
        if (p != lastPlay || ctx.proc.perfSeq.rec.load()) { lastPlay = p; repaint(); }
    }
};

class PerfPage : public juce::Component
{
public:
    explicit PerfPage (Context& c) : ctx (c), toggles (c), seqStrip (c), enabled (c, "ENABLED")
    {
        addAndMakeVisible (toggles); addAndMakeVisible (seqStrip);
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
        auto global = r.removeFromRight (186); r.removeFromRight (6);
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
        // sequencer della Performance in basso, sezioni su due colonne
        auto seqArea = r.removeFromBottom (std::max (150, r.getHeight() / 4)); r.removeFromBottom (6);
        seqStrip.setBounds (seqArea);
        const int rowH = (r.getHeight() - 3 * 6) / 4, colW = (r.getWidth() - 6) / 2;
        int idx = 0;
        auto section = [&] (const char* title, int from, int to)
        {
            const int row = idx % 4, colI = idx / 4; ++idx;
            juce::Rectangle<int> a (r.getX() + colI * (colW + 6), r.getY() + row * (rowH + 6), colW, rowH);
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
        section ("LAZER", mpp::PF_LzLevel, mpp::PF_LzDecay);
        section ("BRK TAPZ / REPEAT", mpp::PF_TzMix, mpp::PF_RpCrush);
    }

private:
    Context& ctx;
    PerfToggles toggles;
    PerfSeqStrip seqStrip;
    juce::OwnedArray<ParamKnob> knobs;
    ParamKnob depth { ctx, "DEPTH" }, rndDepth { ctx, "RANDOMIZE DEPTH" };
    ParamToggle enabled;
    juce::TextButton rndBtn { "DADO" }, commitBtn { "COMMIT" }, resetBtn { "RESET" };
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
};
} // namespace ui
