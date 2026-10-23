#pragma once
#include "Widgets.h"

namespace ui
{
// Disegno delle ripetizioni di BRK TAPZ (posizione e livello di ogni tap)
class TapzView : public juce::Component
{
public:
    explicit TapzView (Context& c) : ctx (c) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2);
        g.setColour (col::bg); g.fillRoundedRectangle (r, 3);
        auto& vs = ctx.proc.dm.vs[ctx.voice];
        mp::BrkTapz t; t.sr = 1000.0f; t.size = 1 << 30;
        t.configure (vs.tzTaps, vs.tzFirst, vs.tzShape, vs.tzDecay, vs.tzSpread);
        const float span = std::max (0.05f, (float) t.span() / 1000.0f * 1.08f);
        const bool on = vs.tzOn;
        g.setColour (col::accent.withAlpha (on ? 1.0f : 0.35f));
        g.fillRect (r.getX() + 2, r.getY() + 4, 3.0f, r.getHeight() - 8);
        for (int k = 0; k < t.nTaps; ++k)
        {
            const float x = r.getX() + 2 + (float) t.dly[k] / 1000.0f / span * (r.getWidth() - 6);
            const float h = (r.getHeight() - 8) * std::max (t.gL[k], t.gR[k]);
            g.setColour (col::accent2.withAlpha (on ? 0.9f : 0.3f));
            g.fillRect (x, r.getBottom() - 4 - h, 2.0f, h);
        }
        g.setColour (col::dim); g.setFont (font (9.5f));
        g.drawText (juce::String (juce::roundToInt (span * 1000.0f / 1.08f)) + " ms", getLocalBounds().reduced (6, 3), juce::Justification::topRight);
    }
private:
    Context& ctx;
};

// Zona da cui trascinare il colpo come WAV nel DAW (o clic per salvarlo)
class DragWav : public juce::Component, public juce::DragAndDropContainer, public juce::SettableTooltipClient
{
public:
    explicit DragWav (Context& c) : ctx (c) { setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Trascina nel DAW il colpo della voce come file WAV (clic: lo salva nella cartella Renders di Drummatic)"); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (over ? col::accent.withAlpha (0.25f) : col::panel2); g.fillRoundedRectangle (r, 4);
        g.setColour (col::accent); g.drawRoundedRectangle (r, 4, 1.2f);
        g.setFont (font (11, true));
        g.drawText (status.isNotEmpty() ? status : juce::String ("TRASCINA WAV"), getLocalBounds(), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        auto f = ctx.proc.renderVoiceToFile (ctx.voice);
        if (f.existsAsFile()) performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        else dragging = false;
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() >= 6) return;
        auto f = ctx.proc.renderVoiceToFile (ctx.voice);
        status = f.existsAsFile() ? "SALVATO" : "ERRORE";
        if (f.existsAsFile()) f.revealToUser();
        repaint();
        juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<DragWav> (this)] { if (safe) { safe->status = {}; safe->repaint(); } });
    }
    juce::String status;
private:
    Context& ctx; bool over = false, dragging = false;
};

// Pagina LAZER: 64 motori glitch, coda, BRK TAPZ e BRK REPEAT della voce selezionata
class LazerPage : public juce::Component, private juce::Timer
{
public:
    explicit LazerPage (Context& c) : ctx (c), tapzView (c), dragWav (c),
        lzOn (c, "LAZER"), lzKit (c, "KIT MODE"), lzEnv (c), tail (c),
        tzOn (c, "BRK TAPZ"), tzSync (c, "SYNC"), tzDiv (c), rpOn (c, "BRK REPEAT"), rpSync (c, "SYNC"), rpDiv (c)
    {
        static const char* titles[mp::kLzKnobs] = { "TUNE", "SWEEP", "S.TIME", "TIMBRE", "COLOR", "MOD", "RATE", "GRIT", "DENSITY", "SPREAD", "DECAY", "LEVEL" };
        for (int k = 0; k < mp::kLzKnobs; ++k) { auto* kn = lzKnobs.add (new ParamKnob (ctx, titles[k])); addAndMakeVisible (kn); allKnobs.push_back (kn); }
        for (auto* k : { &tailLvl, &tailDec, &tailTone, &tzTaps, &tzTime, &tzShape, &tzDecay, &tzSpread, &tzMix,
                         &rpTime, &rpCount, &rpGaps, &rpRev, &rpCrush, &rpMix })
        { addAndMakeVisible (*k); allKnobs.push_back (k); }
        for (auto* b : { &lzOn, &lzKit, &tzOn, &tzSync, &rpOn, &rpSync }) addAndMakeVisible (*b);
        for (auto* cb : { &lzEnv, &tail, &tzDiv, &rpDiv }) addAndMakeVisible (*cb);
        addAndMakeVisible (family); addAndMakeVisible (tapzView); addAndMakeVisible (dragWav); addAndMakeVisible (engineName);
        for (int f = 0; f < mp::kLzFamilies; ++f) family.addItem (mp::lzFamilyName (f), f + 1);
        family.onChange = [this] { setEngine (family.getSelectedItemIndex() * mp::kLzVariants + currentVariant()); };
        for (int vv = 0; vv < mp::kLzVariants; ++vv)
        {
            auto* b = variants.add (new juce::TextButton (juce::String (vv + 1)));
            b->setRadioGroupId (91); b->setClickingTogglesState (true);
            b->onClick = [this, vv] { setEngine (currentFamily() * mp::kLzVariants + vv); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &prevEng, &nextEng, &rndEng, &rndAtmos }) addAndMakeVisible (*b);
        prevEng.onClick = [this] { setEngine ((currentEngine() + mp::kNumLzEngines - 1) % mp::kNumLzEngines); };
        nextEng.onClick = [this] { setEngine ((currentEngine() + 1) % mp::kNumLzEngines); };
        rndEng.onClick = [this] { setEngine (juce::Random::getSystemRandom().nextInt (mp::kNumLzEngines)); };
        rndAtmos.onClick = [this]
        {
            juce::Array<int> a; for (int e = 0; e < mp::kNumLzEngines; ++e) if (mp::lzIsAtmos (e)) a.add (e);
            setEngine (a[juce::Random::getSystemRandom().nextInt (a.size())]);
            // un tappeto lungo e largo
            ctx.proc.setParam (ctx.voice, "lzdecay", 8.0f); ctx.proc.setParam (ctx.voice, "lzspread", 0.8f);
            ctx.proc.setParam (ctx.voice, "lzlevel", -12.0f);
        };
        rndEng.setTooltip ("Motore casuale");
        rndAtmos.setTooltip ("Motore di fondo casuale (Grain Clouds, Shimmer Drones, Static & Radio): coda lunga e larga per l'atmosfera");
        lzKit.setTooltip ("KIT MODE: sul canale MIDI della voce (2-9) ogni tasto suona un motore diverso: nota 36 (C1 in Ableton) = motore 1 ... nota 99 = motore 64");
        lzEnv.setTooltip ("Inviluppo del LAZER: proprio (DECAY) oppure ENV 1 / ENV 2 della voce");
        for (int s = 0; s < mp::kNumSources; ++s) { maps.push_back (std::make_unique<MapButton> (ctx, s)); addAndMakeVisible (*maps.back()); }
        engineName.setFont (font (16, true)); engineName.setColour (juce::Label::textColourId, col::accent);
        rebind();
        startTimerHz (20);
    }

    void rebind()
    {
        for (int k = 0; k < mp::kLzKnobs; ++k) lzKnobs[k]->bind (mpp::targetId (mp::T_LzTune + k), mp::T_LzTune + k);
        lzKnobs[mp::LK_Sweep]->setBipolar (true); lzKnobs[mp::LK_Tune]->setBipolar (true);
        lzOn.bind ("lzOn"); lzKit.bind ("lzKit"); lzEnv.bind ("lzEnv"); tail.bind ("tail");
        tailLvl.bind ("tailLvl"); tailDec.bind ("tailDec"); tailTone.bind ("tailTone");
        tzOn.bind ("tzOn"); tzSync.bind ("tzSync"); tzDiv.bind ("tzDiv"); tzTaps.bind ("tzTaps"); tzTime.bind ("tzTime");
        tzShape.bind ("tzShape"); tzShape.setBipolar (true); tzDecay.bind ("tzDecay"); tzSpread.bind ("tzSpread"); tzMix.bind ("tzMix");
        rpOn.bind ("rpOn"); rpSync.bind ("rpSync"); rpDiv.bind ("rpDiv"); rpTime.bind ("rpTime"); rpCount.bind ("rpCount");
        rpGaps.bind ("rpGaps"); rpRev.bind ("rpRev"); rpCrush.bind ("rpCrush"); rpMix.bind ("rpMix");
        tzShape.textFn = [] (float n) { const float v = n * 2.0f - 1.0f; return std::abs (v) < 0.02f ? juce::String ("costante") : (v > 0 ? "accel " : "rall. ") + juce::String (juce::roundToInt (std::abs (v) * 100)) + "%"; };
        syncEngineUi();
        repaint();
    }
    void armedChanged() { for (auto* k : allKnobs) k->repaint(); for (auto& m : maps) m->repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (auto& p : panels) drawPanel (g, p.first, p.second);
        g.setColour (col::dim); g.setFont (font (10, true));
        g.drawText ("MAP", mapLabel, juce::Justification::centredLeft);
        static const char* sn[] = { "RANDOM", "VELOCITY", "MACRO 1", "MACRO 2", "ENV 1", "ENV 2", "LFO 1", "LFO 2" };
        g.setFont (font (9.5f, true));
        for (size_t i = 0; i < mapNames.size(); ++i)
        {
            g.setColour (ctx.armedSource == (int) i ? col::accent2 : col::dim);
            g.drawText (sn[i], mapNames[i], juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        panels.clear();
        auto r = getLocalBounds().reduced (6);
        const int gap = 6;
        // ---- LAZER ----
        auto lz = r.removeFromTop ((int) (r.getHeight() * 0.56f)); r.removeFromTop (gap);
        panels.push_back ({ lz, "" });
        {
            auto in = lz.reduced (8);
            auto h = in.removeFromTop (24);
            lzOn.setBounds (h.removeFromLeft (84)); h.removeFromLeft (8);
            family.setBounds (h.removeFromLeft (170)); h.removeFromLeft (6);
            for (auto* b : variants) { b->setBounds (h.removeFromLeft (28)); h.removeFromLeft (2); }
            h.removeFromLeft (6);
            prevEng.setBounds (h.removeFromLeft (26)); h.removeFromLeft (2); nextEng.setBounds (h.removeFromLeft (26)); h.removeFromLeft (4);
            rndEng.setBounds (h.removeFromLeft (52)); h.removeFromLeft (4); rndAtmos.setBounds (h.removeFromLeft (84)); h.removeFromLeft (10);
            dragWav.setBounds (h.removeFromRight (120)); h.removeFromRight (8);
            lzKit.setBounds (h.removeFromRight (84)); h.removeFromRight (6);
            lzEnv.setBounds (h.removeFromRight (90)); h.removeFromRight (6);
            in.removeFromTop (4);
            engineName.setBounds (in.removeFromTop (22));
            auto mapRow = in.removeFromBottom (22);
            mapLabel = mapRow.removeFromLeft (36);
            mapNames.clear();
            for (auto& m : maps)
            {
                m->setBounds (mapRow.removeFromLeft (40)); mapRow.removeFromLeft (2);
                mapNames.push_back (mapRow.removeFromLeft (52)); mapRow.removeFromLeft (6);
            }
            in.removeFromBottom (4);
            auto row1 = in.removeFromTop (in.getHeight() / 2), row2 = in;
            for (int k = 0; k < 6; ++k) lzKnobs[k]->setBounds (row1.getX() + k * row1.getWidth() / 6, row1.getY(), row1.getWidth() / 6, row1.getHeight());
            for (int k = 6; k < 12; ++k) lzKnobs[k]->setBounds (row2.getX() + (k - 6) * row2.getWidth() / 6, row2.getY(), row2.getWidth() / 6, row2.getHeight());
        }
        // ---- TAIL / TAPZ / REPEAT ----
        auto tl = r.removeFromLeft (240); r.removeFromLeft (gap);
        auto tz = r.removeFromLeft ((r.getWidth() - gap) / 2); r.removeFromLeft (gap);
        auto rp = r;
        panels.push_back ({ tl, "TAIL" }); panels.push_back ({ tz, "" }); panels.push_back ({ rp, "" });
        {
            auto in = tl.reduced (8); in.removeFromTop (22);
            tail.setBounds (in.removeFromTop (24)); in.removeFromTop (6);
            layoutRow ({ &tailLvl, &tailDec, &tailTone }, in.withSizeKeepingCentre (in.getWidth(), std::min (in.getHeight(), 104)));
        }
        {
            auto in = tz.reduced (8);
            auto h = in.removeFromTop (24);
            tzOn.setBounds (h.removeFromLeft (96)); h.removeFromLeft (6);
            tzDiv.setBounds (h.removeFromRight (74)); h.removeFromRight (4); tzSync.setBounds (h.removeFromRight (52));
            in.removeFromTop (4);
            tapzView.setBounds (in.removeFromTop (34)); in.removeFromTop (4);
            layoutRow ({ &tzTaps, &tzTime, &tzShape, &tzDecay, &tzSpread, &tzMix }, in.withSizeKeepingCentre (in.getWidth(), std::min (in.getHeight(), 104)));
        }
        {
            auto in = rp.reduced (8);
            auto h = in.removeFromTop (24);
            rpOn.setBounds (h.removeFromLeft (106)); h.removeFromLeft (6);
            rpDiv.setBounds (h.removeFromRight (74)); h.removeFromRight (4); rpSync.setBounds (h.removeFromRight (52));
            in.removeFromTop (38);
            layoutRow ({ &rpTime, &rpCount, &rpGaps, &rpRev, &rpCrush, &rpMix }, in.withSizeKeepingCentre (in.getWidth(), std::min (in.getHeight(), 104)));
        }
    }

private:
    Context& ctx;
    TapzView tapzView; DragWav dragWav;
    juce::OwnedArray<ParamKnob> lzKnobs;
    ParamKnob tailLvl { ctx, "LEVEL" }, tailDec { ctx, "DECAY" }, tailTone { ctx, "TONE" },
              tzTaps { ctx, "TAPS" }, tzTime { ctx, "TIME" }, tzShape { ctx, "SHAPE" }, tzDecay { ctx, "DECAY" }, tzSpread { ctx, "SPREAD" }, tzMix { ctx, "MIX" },
              rpTime { ctx, "SLICE" }, rpCount { ctx, "REPEATS" }, rpGaps { ctx, "GAPS" }, rpRev { ctx, "REVERSE" }, rpCrush { ctx, "CRUSH" }, rpMix { ctx, "MIX" };
    ParamToggle lzOn, lzKit; ParamCombo lzEnv, tail;
    ParamToggle tzOn, tzSync; ParamCombo tzDiv;
    ParamToggle rpOn, rpSync; ParamCombo rpDiv;
    juce::ComboBox family; juce::OwnedArray<juce::TextButton> variants;
    juce::TextButton prevEng { "<" }, nextEng { ">" }, rndEng { "DADO" }, rndAtmos { "ATMOSFERA" };
    juce::Label engineName;
    std::vector<std::unique_ptr<MapButton>> maps;
    std::vector<ParamKnob*> allKnobs;
    juce::Rectangle<int> mapLabel; std::vector<juce::Rectangle<int>> mapNames;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
    int shownEngine = -1;

    int currentEngine() { auto* p = ctx.proc.param (ctx.voice, "lzEng"); return juce::roundToInt (p->convertFrom0to1 (p->getValue())); }
    int currentFamily() { return currentEngine() / mp::kLzVariants; }
    int currentVariant() { return currentEngine() % mp::kLzVariants; }
    void setEngine (int e)
    {
        e = juce::jlimit (0, mp::kNumLzEngines - 1, e);
        ctx.proc.setParam (ctx.voice, "lzEng", (float) e);
        if (ctx.proc.param (ctx.voice, "lzOn")->getValue() < 0.5f) ctx.proc.param (ctx.voice, "lzOn")->setValueNotifyingHost (1.0f);
        syncEngineUi();
        ctx.proc.dm.auditionVoice (ctx.voice);       // ascolto immediato del nuovo motore
    }
    void syncEngineUi()
    {
        const int e = currentEngine();
        shownEngine = e;
        family.setSelectedItemIndex (e / mp::kLzVariants, juce::dontSendNotification);
        for (int i = 0; i < variants.size(); ++i) variants[i]->setToggleState (i == e % mp::kLzVariants, juce::dontSendNotification);
        engineName.setText (juce::String (e + 1) + " / 64   " + mp::lzFamilyName (e / mp::kLzVariants) + "  >  " + mp::lzEngineName (e)
                            + (mp::lzIsAtmos (e) ? "      tappeto di fondo: alza DECAY e SPREAD" : ""), juce::dontSendNotification);
    }
    void timerCallback() override
    {
        if (currentEngine() != shownEngine) syncEngineUi();
        const bool tzS = ctx.proc.param (ctx.voice, "tzSync")->getValue() > 0.5f, rpS = ctx.proc.param (ctx.voice, "rpSync")->getValue() > 0.5f;
        tzDiv.setEnabled (tzS); tzTime.setEnabled (! tzS); tzTime.setAlpha (tzS ? 0.35f : 1.0f);
        rpDiv.setEnabled (rpS); rpTime.setEnabled (! rpS); rpTime.setAlpha (rpS ? 0.35f : 1.0f);
        for (auto* k : allKnobs) k->getKnob().repaint();
        tapzView.repaint();
    }
    void layoutRow (std::initializer_list<juce::Component*> comps, juce::Rectangle<int> area)
    {
        const int n = (int) comps.size(); const int w = area.getWidth() / n;
        int i = 0;
        for (auto* c : comps) { c->setBounds (area.getX() + i * w, area.getY(), w, area.getHeight()); ++i; }
    }
};
} // namespace ui
