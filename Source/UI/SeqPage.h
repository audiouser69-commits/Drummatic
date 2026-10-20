#pragma once
#include "Widgets.h"

namespace ui
{
inline mp::SeqSettings seqSettingsFor (ManipoloProcessor& P, int v)
{
    auto iv = [&] (const char* n) { return juce::roundToInt (P.param (v, n)->convertFrom0to1 (P.param (v, n)->getValue())); };
    auto bv = [&] (const char* n) { return P.param (v, n)->getValue() > 0.5f; };
    mp::SeqSettings s;
    s.steps = iv ("steps"); s.fill = iv ("fill"); s.pattern = iv ("pattern"); s.rotate = iv ("rotate"); s.invert = bv ("invert");
    s.length = iv ("length"); s.snap = bv ("snap"); s.resetOn = bv ("resetOn"); s.resetLen = iv ("resetLen");
    return s;
}

// Vista ALL: le sequenze delle 8 voci insieme
class AllView : public juce::Component
{
public:
    explicit AllView (Context& c) : ctx (c) {}
    std::function<void (int)> onSelectVoice;
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4);
        const float rowH = (float) getHeight() / mp::kNumVoices;
        int maxL = 1;
        for (int v = 0; v < mp::kNumVoices; ++v) maxL = std::max (maxL, mp::sequenceLength (seqSettingsFor (ctx.proc, v)));
        const float w = (getWidth() - 40.0f) / (float) maxL;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            const auto s = seqSettingsFor (ctx.proc, v);
            const int L = mp::sequenceLength (s);
            auto& sq = ctx.proc.dm.sshared[v];
            const float y = v * rowH;
            g.setColour (v == ctx.voice ? col::accent : col::dim); g.setFont (font (15, true));
            g.drawText (juce::String (v + 1), 4, (int) y, 28, (int) rowH, juce::Justification::centred);
            const bool muted = ctx.proc.dm.mute[v].load();
            const int play = sq.playStep.load();
            for (int i = 0; i < L; ++i)
            {
                auto cell = juce::Rectangle<float> (36 + i * w, y + 3, w, rowH - 6).reduced (std::min (2.0f, w * 0.12f), 0);
                g.setColour (i == play ? col::text.withAlpha (0.22f) : (i % 4 == 0 ? col::panel2.brighter (0.05f) : col::panel2));
                g.fillRoundedRectangle (cell, 2);
                const bool man = sq.manual[i].load(), pat = mp::patternHit (s, i % std::max (1, s.steps));
                if (man || pat)
                {
                    g.setColour ((man ? col::accent : col::accent2).withAlpha (muted ? 0.3f : 0.9f));
                    const float d = std::min (cell.getWidth(), cell.getHeight()) * 0.55f;
                    g.fillEllipse (cell.withSizeKeepingCentre (d, d));
                }
            }
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float rowH = (float) getHeight() / mp::kNumVoices;
        const int v = juce::jlimit (0, mp::kNumVoices - 1, (int) (e.y / rowH));
        if (e.x < 36) { if (onSelectVoice) onSelectVoice (v); return; }
        int maxL = 1;
        for (int k = 0; k < mp::kNumVoices; ++k) maxL = std::max (maxL, mp::sequenceLength (seqSettingsFor (ctx.proc, k)));
        const float w = (getWidth() - 40.0f) / (float) maxL;
        const int i = (int) ((e.x - 36) / w);
        if (i >= 0 && i < mp::sequenceLength (seqSettingsFor (ctx.proc, v)))
        {
            auto& m = ctx.proc.dm.sshared[v].manual[i];
            m = ! m.load(); repaint();
        }
    }
private:
    Context& ctx;
};

// Griglia passi + editor della lane selezionata
class StepEditor : public juce::Component
{
public:
    explicit StepEditor (Context& c) : ctx (c) {}
    int lane = mp::L_Vel;

    mp::SeqSettings settings() { return seqSettingsFor (ctx.proc, ctx.voice); }
    int visibleSteps()
    {
        auto& sq = ctx.proc.dm.sshared[ctx.voice];
        const int sync = sq.laneSync[lane].load();
        if (sync == mp::LS_Sync) return mp::sequenceLength (settings());
        return std::max (1, sq.laneLen[lane].load());
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (r, 4);
        const auto s = settings();
        const int L = mp::sequenceLength (s);
        const int nVis = visibleSteps();
        auto& sq = ctx.proc.dm.sshared[ctx.voice];
        const float trigH = 26.0f;
        const int shown = std::max (L, nVis);
        const float w = (r.getWidth() - 8) / (float) shown;
        const int play = sq.playStep.load();

        // riga trigger
        for (int i = 0; i < L; ++i)
        {
            const bool man = sq.manual[i].load();
            const bool pat = mp::patternHit (s, i % std::max (1, s.steps));
            auto cell = juce::Rectangle<float> (4 + i * w, 4, w, trigH).reduced (std::min (2.0f, w * 0.15f), 3);
            g.setColour (i == play ? col::text.withAlpha (0.25f) : col::bg); g.fillRoundedRectangle (cell, 2);
            if (man || pat)
            {
                g.setColour (man ? col::accent : col::accent2.withAlpha (0.8f));
                g.fillEllipse (cell.withSizeKeepingCentre (std::min (cell.getWidth(), 10.0f), std::min (cell.getWidth(), 10.0f)));
            }
            if (i % 4 == 0) { g.setColour (col::line); g.drawVerticalLine ((int) (4 + i * w), 2, r.getBottom() - 2); }
        }
        // editor della lane
        const auto& li = mp::laneInfo (lane);
        auto area = juce::Rectangle<float> (4, trigH + 10, r.getWidth() - 8, r.getHeight() - trigH - 14);
        const bool bip = li.lo < 0;
        const float zeroY = bip ? area.getCentreY() : area.getBottom();
        g.setColour (col::line); g.drawHorizontalLine ((int) zeroY, area.getX(), area.getRight());
        for (int i = 0; i < nVis; ++i)
        {
            const float v = sq.lane[lane][i].load();
            const float n = (v - li.lo) / (li.hi - li.lo);
            const float y = area.getBottom() - n * area.getHeight();
            auto bar = juce::Rectangle<float>::leftTopRightBottom (area.getX() + i * w + 1, std::min (y, zeroY), area.getX() + (i + 1) * w - 1, std::max (y, zeroY));
            if (bar.getHeight() < 2) bar = bar.withHeight (2).withY (zeroY - 1);
            const bool isDefault = std::abs (v - li.def) < 0.001f;
            g.setColour (isDefault ? col::dim.withAlpha (0.35f) : col::accent.withAlpha (0.85f));
            if (lane == mp::L_Vel && v >= 127) g.setColour (col::warn);
            g.fillRect (bar);
            if (w > 22)
            {
                g.setColour (col::dim); g.setFont (font (9.5f));
                g.drawText (juce::String (juce::roundToInt (v)), (int) (area.getX() + i * w), (int) area.getBottom() - 12, (int) w, 12, juce::Justification::centred);
            }
        }
        g.setColour (col::dim); g.setFont (font (11, true));
        g.drawText (li.name, area.toNearestInt().reduced (4), juce::Justification::topRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < 32)
        {
            const int i = stepAt (e.x, mp::sequenceLength (settings()));
            if (i >= 0) { auto& m = ctx.proc.dm.sshared[ctx.voice].manual[i]; paintState = ! m.load(); m = paintState; }
            drawingTriggers = true;
        }
        else { drawingTriggers = false; editLane (e); }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drawingTriggers)
        {
            const int i = stepAt (e.x, mp::sequenceLength (settings()));
            if (i >= 0) ctx.proc.dm.sshared[ctx.voice].manual[i] = paintState;
        }
        else editLane (e);
        repaint();
    }

private:
    Context& ctx; bool drawingTriggers = false, paintState = true;
    int stepAt (int x, int L)
    {
        const int shown = std::max (L, visibleSteps());
        const float w = (getWidth() - 8) / (float) shown;
        const int i = (int) ((x - 4) / w);
        return i >= 0 && i < L ? i : -1;
    }
    void editLane (const juce::MouseEvent& e)
    {
        const int n = visibleSteps();
        const int shown = std::max (mp::sequenceLength (settings()), n);
        const float w = (getWidth() - 8) / (float) shown;
        const int i = (int) ((e.x - 4) / w);
        if (i < 0 || i >= n) return;
        const auto& li = mp::laneInfo (lane);
        float v;
        if (e.mods.isAltDown()) v = li.def;
        else
        {
            const float top = 36, bottom = (float) getHeight() - 4;
            const float nn = juce::jlimit (0.0f, 1.0f, (bottom - (float) e.y) / (bottom - top));
            v = li.lo + nn * (li.hi - li.lo);
            if (li.integer) v = std::round (v);
        }
        ctx.proc.dm.sshared[ctx.voice].lane[lane][i] = v;
    }
};

class SeqPage : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onSelectVoice;
    explicit SeqPage (Context& c) : ctx (c), editor (c), all (c), invert (c, "INVERT"), snap (c, "SNAP"), resetOn (c, "RESET"),
                                   speed (c), mult (c)
    {
        for (auto* k : { &steps, &fill, &pattern, &rotate, &shift, &swing, &shape, &density, &length, &resetLen, &humanize })
            addAndMakeVisible (*k);
        for (auto* k : { &shift, &swing, &shape, &density }) k->setBipolar (true);
        addAndMakeVisible (invert); addAndMakeVisible (snap); addAndMakeVisible (resetOn);
        addAndMakeVisible (speed); addAndMakeVisible (mult); addAndMakeVisible (editor);
        addChildComponent (all); addAndMakeVisible (allBtn);
        allBtn.setClickingTogglesState (true);
        allBtn.setTooltip ("Vista ALL: tutte le voci insieme (clic sul numero per selezionare la voce)");
        allBtn.onClick = [this] { showAll (allBtn.getToggleState()); };
        all.onSelectVoice = [this] (int v) { if (onSelectVoice) onSelectVoice (v); };
        for (int l = 0; l < mp::kNumLanes; ++l)
        {
            auto* b = laneBtns.add (new juce::TextButton (mp::laneInfo (l).name));
            b->setRadioGroupId (42); b->setClickingTogglesState (true);
            b->onClick = [this, l] { editor.lane = l; updateLaneControls(); editor.repaint(); };
            addAndMakeVisible (b);
        }
        laneBtns[0]->setToggleState (true, juce::dontSendNotification);
        for (auto* b : { &clearBtn, &leftBtn, &rightBtn, &diceBtn, &clearTrigBtn }) addAndMakeVisible (*b);
        addAndMakeVisible (laneSync); addAndMakeVisible (laneLen);
        laneSync.addItemList ({ "SYNC", "FREE", "TRIG", "RETRIG" }, 1);
        for (int i = 1; i <= mp::kMaxSteps; ++i) laneLen.addItem (juce::String (i), i);
        laneSync.onChange = [this] { ctx.proc.dm.sshared[ctx.voice].laneSync[editor.lane] = laneSync.getSelectedItemIndex(); editor.repaint(); };
        laneLen.onChange = [this] { ctx.proc.dm.sshared[ctx.voice].laneLen[editor.lane] = laneLen.getSelectedId(); editor.repaint(); };
        clearBtn.onClick = [this] { ctx.proc.dm.sshared[ctx.voice].clearLane (editor.lane); editor.repaint(); };
        clearTrigBtn.onClick = [this] { for (auto& m : ctx.proc.dm.sshared[ctx.voice].manual) m = false; editor.repaint(); };
        leftBtn.onClick = [this] { shiftLane (-1); };
        rightBtn.onClick = [this] { shiftLane (1); };
        diceBtn.onClick = [this] { randomLane(); };
        clearBtn.setTooltip ("Azzera la lane visibile");
        clearTrigBtn.setTooltip ("Cancella i trigger manuali");
        leftBtn.setTooltip ("Ruota la lane a sinistra (Alt: anche i trigger manuali)");
        rightBtn.setTooltip ("Ruota la lane a destra (Alt: anche i trigger manuali)");
        rebind();
        startTimerHz (30);
    }

    void rebind()
    {
        steps.bind ("steps"); fill.bind ("fill"); pattern.bind ("pattern"); rotate.bind ("rotate");
        shift.bind ("sshift"); swing.bind ("swing"); shape.bind ("shape"); density.bind ("density");
        length.bind ("length"); resetLen.bind ("resetLen"); humanize.bind ("humanize", -1, false);
        invert.bind ("invert"); snap.bind ("snap"); resetOn.bind ("resetOn"); speed.bind ("speed"); mult.bind ("mult");
        updateLaneControls(); editor.repaint(); all.repaint();
    }

    void paint (juce::Graphics& g) override { for (auto& p : panels) drawPanel (g, p.first, p.second); }

    void resized() override
    {
        panels.clear();
        auto r = getLocalBounds().reduced (6);
        auto top = r.removeFromTop (130); r.removeFromTop (6);
        auto gen = top.removeFromLeft (330); top.removeFromLeft (6);
        auto mod = top.removeFromLeft (300); top.removeFromLeft (6);
        auto hum = top.removeFromRight (100); top.removeFromRight (6);
        auto set = top;
        panels.push_back ({ gen, "GENERATE" }); panels.push_back ({ mod, "MODIFY" }); panels.push_back ({ set, "SETTINGS" }); panels.push_back ({ hum, "HUMANIZE" });
        invert.setBounds (gen.getRight() - 72, gen.getY() + 3, 64, 18);
        row ({ &steps, &fill, &pattern, &rotate }, gen.reduced (4).withTrimmedTop (26));
        row ({ &shift, &swing, &shape, &density }, mod.reduced (4).withTrimmedTop (26));
        humanize.setBounds (hum.reduced (4).withTrimmedTop (26));
        {
            auto in = set.reduced (6).withTrimmedTop (24);
            auto c1 = in.removeFromLeft (in.getWidth() / 3);
            auto c2 = in.removeFromLeft (in.getWidth() / 2);
            auto c3 = in;
            length.setBounds (c1);
            speed.setBounds (c2.removeFromTop (26).reduced (2)); mult.setBounds (c2.removeFromTop (26).reduced (2)); snap.setBounds (c2.removeFromTop (26).reduced (2));
            resetOn.setBounds (c3.removeFromTop (24).reduced (2)); resetLen.setBounds (c3);
        }
        auto bottom = r.removeFromBottom (28); r.removeFromBottom (4);
        auto laneRow = r.removeFromBottom (26); r.removeFromBottom (4);
        editor.setBounds (r); all.setBounds (r.getUnion (laneRow).withBottom (bottom.getY() - 4));
        const int lw = laneRow.getWidth() / mp::kNumLanes;
        for (int l = 0; l < mp::kNumLanes; ++l) laneBtns[l]->setBounds (laneRow.removeFromLeft (lw).reduced (2, 0));
        clearBtn.setBounds (bottom.removeFromLeft (34).reduced (2)); leftBtn.setBounds (bottom.removeFromLeft (30).reduced (2));
        rightBtn.setBounds (bottom.removeFromLeft (30).reduced (2)); diceBtn.setBounds (bottom.removeFromLeft (54).reduced (2));
        clearTrigBtn.setBounds (bottom.removeFromLeft (130).reduced (2));
        allBtn.setBounds (bottom.removeFromLeft (60).reduced (2));
        laneLen.setBounds (bottom.removeFromRight (70).reduced (2)); laneSync.setBounds (bottom.removeFromRight (100).reduced (2));
    }

private:
    Context& ctx;
    StepEditor editor;
    AllView all;
    juce::TextButton allBtn { "ALL" };
    void showAll (bool on)
    {
        all.setVisible (on); editor.setVisible (! on);
        for (auto* b : laneBtns) b->setVisible (! on);
        for (auto* c : { (juce::Component*) &clearBtn, (juce::Component*) &leftBtn, (juce::Component*) &rightBtn, (juce::Component*) &diceBtn,
                         (juce::Component*) &laneSync, (juce::Component*) &laneLen })
            c->setVisible (! on);
    }
    ParamKnob steps { ctx, "STEPS" }, fill { ctx, "FILL" }, pattern { ctx, "PATTERN" }, rotate { ctx, "ROTATE" },
              shift { ctx, "SHIFT" }, swing { ctx, "SWING" }, shape { ctx, "SHAPE" }, density { ctx, "DENSITY" },
              length { ctx, "LENGTH" }, resetLen { ctx, "RESET LEN" }, humanize { ctx, "AMOUNT" };
    ParamToggle invert, snap, resetOn;
    ParamCombo speed, mult;
    juce::OwnedArray<juce::TextButton> laneBtns;
    juce::TextButton clearBtn { "X" }, leftBtn { "<" }, rightBtn { ">" }, diceBtn { "DADO" }, clearTrigBtn { "CANCELLA TRIGGER" };
    juce::ComboBox laneSync, laneLen;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;

    void timerCallback() override { if (all.isVisible()) all.repaint(); else editor.repaint(); }
    void row (std::initializer_list<juce::Component*> comps, juce::Rectangle<int> a)
    {
        const int w = a.getWidth() / (int) comps.size(); int i = 0;
        for (auto* c : comps) c->setBounds (a.getX() + w * i++, a.getY(), w, a.getHeight());
    }
    void updateLaneControls()
    {
        auto& sq = ctx.proc.dm.sshared[ctx.voice];
        laneSync.setSelectedItemIndex (sq.laneSync[editor.lane].load(), juce::dontSendNotification);
        laneLen.setSelectedId (sq.laneLen[editor.lane].load(), juce::dontSendNotification);
    }
    void shiftLane (int dir)
    {
        auto& sq = ctx.proc.dm.sshared[ctx.voice];
        const int n = editor.visibleSteps();
        std::vector<float> tmp ((size_t) n);
        for (int i = 0; i < n; ++i) tmp[(size_t) i] = sq.lane[editor.lane][i].load();
        for (int i = 0; i < n; ++i) sq.lane[editor.lane][((i + dir) % n + n) % n] = tmp[(size_t) i];
        if (juce::ModifierKeys::currentModifiers.isAltDown())
        {
            const int L = mp::sequenceLength (editor.settings());
            std::vector<bool> t ((size_t) L);
            for (int i = 0; i < L; ++i) t[(size_t) i] = sq.manual[i].load();
            for (int i = 0; i < L; ++i) sq.manual[((i + dir) % L + L) % L] = t[(size_t) i];
        }
        editor.repaint();
    }
    void randomLane()
    {
        auto& sq = ctx.proc.dm.sshared[ctx.voice];
        const auto& li = mp::laneInfo (editor.lane);
        const float depth = ctx.proc.apvts.getRawParameterValue ("rnd_depth")->load();
        auto& rnd = juce::Random::getSystemRandom();
        for (int i = 0; i < editor.visibleSteps(); ++i)
        {
            const float cur = sq.lane[editor.lane][i].load();
            float v = cur + (rnd.nextFloat() * 2 - 1) * depth * (li.hi - li.lo);
            v = juce::jlimit (li.lo, li.hi, v);
            if (li.integer) v = std::round (v);
            sq.lane[editor.lane][i] = v;
        }
        editor.repaint();
    }
};
} // namespace ui
