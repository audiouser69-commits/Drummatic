#pragma once
// Schermata 1280 x 720 per il display da 5" della macchina: segue pagina, banco e voce del pannello.
#include "HardwareController.h"
#include "../UI/Theme.h"

namespace hw
{
class Screen : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1280, H = 720;
    explicit Screen (Controller& c) : ctl (c) { setSize (W, H); setOpaque (true); startTimerHz (30); }

    std::function<void()> onFrame;      // l'app aggiorna i LED e lo stato a ogni fotogramma

    void paint (juce::Graphics& g) override
    {
        using namespace ui;
        g.fillAll (col::bg);
        paintHeader (g, { 0, 0, W, 72 });
        const auto& bank = ctl.currentBank();
        const int tx = 24, ty = 88, tw = (W - 48 - 3 * 16) / 4, th = 196;
        for (int i = 0; i < 8; ++i)
            paintTile (g, { tx + (i % 4) * (tw + 16), ty + (i / 4) * (th + 16), tw, th }, bank[(size_t) i], i);
        juce::Rectangle<int> bottom (24, ty + 2 * th + 32, W - 48, H - (ty + 2 * th + 32) - 18 - kFooterH);
        if (ctl.page == P_Smpl) paintWave (g, bottom.removeFromLeft (860));
        else if (ctl.page == P_Mix) paintMixer (g, bottom.removeFromLeft (860));
        else paintSteps (g, bottom.removeFromLeft (860));
        bottom.removeFromLeft (16);
        paintMeters (g, bottom);
        paintFooter (g, { 0, H - kFooterH, W, kFooterH });
        if (ctl.showToast()) paintToast (g);
        if (! panelConnected)
        {
            g.setColour (col::warn); g.setFont (font (15, true));
            g.drawText ("CIRCUITO DI CONTROLLO NON COLLEGATO", 0, H - kFooterH - 18, W, 16, juce::Justification::centred);
        }
    }

    bool panelConnected = true;
    juce::String audioStatus;

private:
    Controller& ctl;

    void timerCallback() override { if (onFrame) onFrame(); repaint(); }

    // la riga dei tasti senza scritta sul pannello (revisione 26/10/2026: sul pannello restano solo numeri, trasporto,
    // FUNC e frecce): le 8 pagine e i tasti funzione, nello stesso ordine della fila di tasti sotto il display
    static constexpr int kFooterH = 40;
    void paintFooter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRect (r);
        g.setColour (col::line); g.fillRect (r.removeFromTop (2));
        static const char* funcNames[] = { "FUNC", "MAP", "LOCK", "DADO", "COMMIT", "KIT", "COPY" };
        const float w = r.getWidth() / 15.0f;
        for (int i = 0; i < 15; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * w, (float) r.getY(), w, (float) r.getHeight()).reduced (3, 5);
            bool on = false; juce::String name;
            if (i < kNumPages) { name = pageName (i); on = i == ctl.page; }
            else
            {
                const int f = i - kNumPages; name = funcNames[f];
                on = (f == 0 && ctl.func) || (f == 1 && ctl.mapMode) || (f == 2 && ctl.lastSlotLocked());
            }
            if (on) { g.setColour (i < kNumPages ? col::accent : col::accent2); g.fillRoundedRectangle (c, 5); }
            else if (i == kNumPages) { g.setColour (col::line); g.fillRect (c.getX() - 3.0f, c.getY(), 1.0f, c.getHeight()); }
            g.setColour (on ? juce::Colours::black : (i < kNumPages ? col::text : col::dim));
            g.setFont (font (15, true));
            g.drawFittedText (name, c.toNearestInt(), juce::Justification::centred, 1);
        }
    }

    void paintHeader (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRect (r);
        g.setColour (col::line); g.fillRect (r.removeFromBottom (2));
        r.reduce (24, 0);
        g.setColour (col::dim); g.setFont (font (22, true));
        g.drawText ("D R U M M A T I C", r.removeFromLeft (250), juce::Justification::centredLeft);
        // pagina
        auto pill = r.removeFromLeft (190).reduced (0, 16);
        g.setColour (ctl.mapMode ? col::accent2 : col::accent); g.fillRoundedRectangle (pill.toFloat(), 8);
        g.setColour (juce::Colours::black); g.setFont (font (24, true));
        g.drawText (ctl.mapMode ? juce::String ("MAP") : juce::String (pageName (ctl.page)), pill, juce::Justification::centred);
        r.removeFromLeft (16);
        g.setColour (col::text); g.setFont (font (20, true));
        juce::String bankTxt = ctl.mapMode ? juce::String ("SORGENTE  ") + mp::sourceName (ctl.mapSource) : ctl.bankName();
        if (! ctl.mapMode && ctl.numBanks() > 1) bankTxt << "   " << (ctl.bank[(size_t) ctl.page] + 1) << "/" << ctl.numBanks();
        g.drawText (bankTxt, r.removeFromLeft (330), juce::Justification::centredLeft);
        // destra: trasporto, tempo, voce
        auto right = r.removeFromRight (420);
        const bool running = ctl.proc.dm.running.load();
        auto icon = right.removeFromRight (44).reduced (8, 22).toFloat();
        if (running) { juce::Path p; p.addTriangle (icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY()); g.setColour (juce::Colour (0xff6fd16f)); g.fillPath (p); }
        else { g.setColour (col::dim); g.fillRect (icon.reduced (2)); }
        if (ctl.recArmed) { g.setColour (col::warn); g.fillEllipse (right.removeFromRight (30).reduced (6, 26).toFloat()); }
        g.setColour (col::text); g.setFont (font (22, true));
        const float bpm = ctl.proc.apvts.getRawParameterValue ("bpm")->load();
        g.drawText (juce::String (bpm, 1) + " BPM", right.removeFromRight (150), juce::Justification::centredRight);
        g.setColour (col::accent); g.setFont (font (22, true));
        g.drawText ("VOCE " + juce::String (ctl.voice + 1) + "  " + ctl.engineName().toUpperCase(), right, juce::Justification::centredLeft);
    }

    void paintTile (juce::Graphics& g, juce::Rectangle<int> r, const Slot& s, int idx)
    {
        using namespace ui;
        const bool touched = idx == ctl.lastSlot && juce::Time::getMillisecondCounter() - ctl.lastTouchMs < 1500;
        g.setColour (touched ? col::panel2.brighter (0.08f) : col::panel); g.fillRoundedRectangle (r.toFloat(), 12);
        g.setColour (touched ? col::accent : col::line); g.drawRoundedRectangle (r.toFloat().reduced (1), 12, touched ? 2.5f : 1.0f);
        if (s.kind == Slot::None) return;
        auto in = r.reduced (16, 12);
        g.setColour (col::dim); g.setFont (font (18, true));
        g.drawText (juce::String (idx + 1), in.removeFromTop (22).removeFromRight (30), juce::Justification::topRight);
        in = r.reduced (16, 12);
        g.setColour (col::text); g.setFont (font (21, true));
        g.drawText (s.label, in.removeFromTop (26), juce::Justification::topLeft);
        // arco
        auto arcBox = in.removeFromTop (104).toFloat().withSizeKeepingCentre (100, 100);
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        const float n = juce::jlimit (0.0f, 1.0f, ctl.slotNorm (s));
        const float cx = arcBox.getCentreX(), cy = arcBox.getCentreY(), rad = 44;
        juce::Path track; track.addCentredArc (cx, cy, rad, rad, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (8, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float from = ctl.slotBipolar (s) ? (a0 + a1) * 0.5f : a0, to = a0 + (a1 - a0) * n;
        juce::Path val; val.addCentredArc (cx, cy, rad, rad, 0, std::min (from, to), std::max (from, to) + 0.001f, true);
        g.setColour (col::accent); g.strokePath (val, juce::PathStrokeType (8, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (s.target >= 0)
        {
            const float d = ctl.slotDepth (s);
            if (std::abs (d) > 0.001f || ctl.mapMode)
            {
                const float m = juce::jlimit (a0, a1, to + d * (a1 - a0));
                juce::Path dep; dep.addCentredArc (cx, cy, rad - 13, rad - 13, 0, std::min (to, m), std::max (to, m) + 0.001f, true);
                g.setColour (col::accent2); g.strokePath (dep, juce::PathStrokeType (5, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            if (ctl.proc.dm.vshared[ctl.slotVoice (s)].locked[s.target].load()) paintLock (g, { r.getRight() - 40, r.getY() + 44, 18, 20 });
        }
        if (s.perf >= 0 && ctl.proc.perfLock[s.perf].load()) paintLock (g, { r.getRight() - 40, r.getY() + 44, 18, 20 });
        // valore
        juce::String txt = ctl.mapMode && s.target >= 0 ? "MOD " + juce::String (juce::roundToInt (ctl.slotDepth (s) * 100)) + " %" : ctl.slotText (s);
        if (s.kind == Slot::SampleFile && ctl.pendingSampleName().isNotEmpty()) txt = ctl.pendingSampleName();
        g.setColour (ctl.mapMode && s.target >= 0 ? col::accent2 : col::text);
        g.setFont (font (txt.length() > 14 ? 19.0f : 25.0f, true));
        g.drawFittedText (txt, in.reduced (0, 4), juce::Justification::centred, 1);
    }

    static void paintLock (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (ui::col::warn);
        g.fillRoundedRectangle (r.toFloat().withTrimmedTop (8), 2);
        juce::Path p; p.addCentredArc ((float) r.getCentreX(), (float) r.getY() + 8, 6, 6, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (p, juce::PathStrokeType (2.5f));
    }

    void paintSteps (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 12);
        auto head = r.reduced (16, 10).removeFromTop (26);
        g.setColour (col::dim); g.setFont (font (18, true));
        const int L = ctl.seqLength();
        juce::String title = ctl.page == P_Lane ? juce::String ("LANE ") + mp::laneInfo (ctl.lane).name + "   passo " + juce::String (ctl.laneStep + 1)
                                                : "SEQUENZA VOCE " + juce::String (ctl.voice + 1);
        g.drawText (title, head, juce::Justification::centredLeft);
        g.drawText ("PASSI " + juce::String (ctl.stepPage * 16 + 1) + "-" + juce::String (std::min (L, ctl.stepPage * 16 + 16)) + " DI " + juce::String (L),
                    head, juce::Justification::centredRight);
        auto cells = r.reduced (16, 10).withTrimmedTop (34);
        const float w = cells.getWidth() / 16.0f;
        const int play = ctl.proc.dm.sshared[ctl.voice].playStep.load();
        const auto& li = mp::laneInfo (ctl.lane);
        for (int i = 0; i < 16; ++i)
        {
            const int s = ctl.stepPage * 16 + i;
            auto c = juce::Rectangle<float> (cells.getX() + i * w, (float) cells.getY(), w, (float) cells.getHeight()).reduced (4, 2);
            if (s >= L) { g.setColour (col::bg); g.fillRoundedRectangle (c, 6); continue; }
            g.setColour (i % 4 == 0 ? col::panel2.brighter (0.06f) : col::panel2); g.fillRoundedRectangle (c, 6);
            if (ctl.page == P_Lane)
            {
                const float v = ctl.proc.dm.sshared[ctl.voice].lane[ctl.lane][s].load();
                const float zero = li.lo < 0 ? 0.0f : li.lo;
                const float y0 = c.getBottom() - (zero - li.lo) / (li.hi - li.lo) * c.getHeight();
                const float y1 = c.getBottom() - (v - li.lo) / (li.hi - li.lo) * c.getHeight();
                g.setColour (s == ctl.laneStep ? col::accent2 : col::accent2.withAlpha (0.55f));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (c.getX() + 6, std::min (y0, y1), c.getRight() - 6, std::max (y0, y1) + 1));
            }
            else
            {
                const bool on = ctl.stepOn (s), pat = ctl.stepPattern (s);
                if (on || pat)
                {
                    g.setColour (on ? col::accent : col::accent.withAlpha (0.45f));
                    g.fillRoundedRectangle (c.reduced (8, c.getHeight() * 0.2f), 6);
                }
            }
            if (s == play && ctl.proc.dm.running.load()) { g.setColour (col::text); g.drawRoundedRectangle (c, 6, 3); }
        }
    }

    void paintWave (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 12);
        auto in = r.reduced (16, 10);
        auto f = ctl.proc.getSampleFile (ctl.voice);
        g.setColour (col::dim); g.setFont (font (18, true));
        g.drawText (f.existsAsFile() ? f.getFileName() : juce::String ("NESSUN CAMPIONE - BANCO 2, ENCODER 1 PER SCEGLIERE"), in.removeFromTop (26), juce::Justification::centredLeft);
        auto data = ctl.proc.getSample (ctl.voice);
        if (data == nullptr || data->length() == 0) return;
        auto wave = in.withTrimmedTop (6).toFloat();
        const int n = data->length(), cols = (int) wave.getWidth();
        g.setColour (col::accent.withAlpha (0.8f));
        for (int x = 0; x < cols; ++x)
        {
            const int a = (int) ((juce::int64) n * x / cols), b = std::max (a + 1, (int) ((juce::int64) n * (x + 1) / cols));
            float pk = 0; for (int i = a; i < b; i += std::max (1, (b - a) / 64)) pk = std::max (pk, std::abs (data->l[(size_t) i]));
            const float h = pk * wave.getHeight() * 0.5f;
            g.fillRect (wave.getX() + x, wave.getCentreY() - h, 1.0f, std::max (1.0f, 2 * h));
        }
        auto* ps = ctl.proc.param (ctl.voice, "start"); auto* pe = ctl.proc.param (ctl.voice, "end");
        const float xs = wave.getX() + ps->getValue() * wave.getWidth(), xe = wave.getX() + pe->getValue() * wave.getWidth();
        g.setColour (col::accent2); g.fillRect (xs - 1.5f, wave.getY(), 3.0f, wave.getHeight());
        g.setColour (col::warn); g.fillRect (xe - 1.5f, wave.getY(), 3.0f, wave.getHeight());
    }

    void paintMixer (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 12);
        auto in = r.reduced (16, 10);
        const float w = in.getWidth() / 8.0f;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            auto c = juce::Rectangle<float> (in.getX() + v * w, (float) in.getY(), w, (float) in.getHeight()).reduced (6, 0);
            const bool muted = ctl.proc.dm.mute[v].load();
            g.setColour (muted ? col::warn.withAlpha (0.25f) : (v == ctl.voice ? col::panel2.brighter (0.1f) : col::panel2));
            g.fillRoundedRectangle (c, 8);
            g.setColour (muted ? col::warn : col::text); g.setFont (font (22, true));
            g.drawText (juce::String (v + 1), c.removeFromTop (30), juce::Justification::centred);
            g.setColour (col::dim); g.setFont (font (15, true));
            g.drawText (muted ? "MUTE" : ctl.proc.param (v, "gain")->getCurrentValueAsText(), c.removeFromBottom (24), juce::Justification::centred);
        }
    }

    void paintMeters (juce::Graphics& g, juce::Rectangle<int> r)
    {
        using namespace ui;
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 12);
        auto in = r.reduced (14, 12);
        const float w = in.getWidth() / 8.0f;
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            auto c = juce::Rectangle<float> (in.getX() + v * w, (float) in.getY(), w, (float) in.getHeight()).reduced (5, 0);
            auto label = c.removeFromBottom (22);
            g.setColour (col::panel2); g.fillRoundedRectangle (c, 4);
            const float lvl = juce::jlimit (0.0f, 1.0f, std::sqrt (ctl.proc.dm.vshared[v].level.load()));
            const bool muted = ctl.proc.dm.mute[v].load();
            g.setColour (muted ? col::warn.withAlpha (0.5f) : (v == ctl.voice ? col::accent : col::accent2));
            g.fillRoundedRectangle (c.withTrimmedTop (c.getHeight() * (1 - lvl)), 4);
            g.setColour (v == ctl.voice ? col::accent : col::dim); g.setFont (font (16, true));
            g.drawText (juce::String (v + 1), label, juce::Justification::centred);
        }
    }

    void paintToast (juce::Graphics& g)
    {
        using namespace ui;
        auto r = juce::Rectangle<float> (0, 0, 700, 90).withCentre ({ W / 2.0f, H / 2.0f });
        g.setColour (juce::Colours::black.withAlpha (0.82f)); g.fillRoundedRectangle (r, 16);
        g.setColour (col::accent); g.drawRoundedRectangle (r, 16, 2);
        g.setColour (col::text); g.setFont (font (30, true));
        g.drawFittedText (ctl.toast, r.toNearestInt().reduced (20), juce::Justification::centred, 2);
    }
};
} // namespace hw
