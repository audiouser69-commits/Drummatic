#pragma once
#include "Widgets.h"

namespace ui
{
// Forma d'onda del campione con marcatori START/END trascinabili e drag&drop di file
class WaveView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit WaveView (Context& c) : ctx (c) {}
    std::function<void()> onSampleChanged;

    void refresh()
    {
        data = ctx.proc.getSample (ctx.voice);
        peaks.clear();
        if (data && data->length() > 0)
        {
            const int w = std::max (1, getWidth());
            peaks.resize ((size_t) w);
            const int n = data->length();
            for (int x = 0; x < w; ++x)
            {
                const int a = (int) ((juce::int64) x * n / w), b = std::max (a + 1, (int) ((juce::int64) (x + 1) * n / w));
                float mx = 0;
                for (int i = a; i < std::min (b, n); ++i) mx = std::max (mx, std::abs (data->l[(size_t) i]));
                peaks[(size_t) x] = mx;
            }
        }
        repaint();
    }
    void resized() override { refresh(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg); g.fillRoundedRectangle (r, 3);
        const float s = start(), e = end();
        const float xs = r.getX() + s * r.getWidth(), xe = r.getX() + e * r.getWidth();
        g.setColour (col::accent.withAlpha (0.07f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (xs, xe), r.getY(), std::max (xs, xe), r.getBottom()));
        if (peaks.empty())
        {
            g.setColour (col::dim); g.setFont (font (12));
            g.drawText ("Trascina qui un file audio o clicca CARICA", getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            const float mid = r.getCentreY(), hh = r.getHeight() * 0.45f;
            g.setColour (col::accent.withAlpha (0.85f));
            for (size_t x = 0; x < peaks.size(); ++x)
                g.drawVerticalLine ((int) x, mid - peaks[x] * hh, mid + peaks[x] * hh);
        }
        g.setColour (col::accent2);
        g.drawVerticalLine ((int) xs, r.getY(), r.getBottom()); g.fillEllipse (xs - 5, r.getY(), 10, 10);
        g.drawVerticalLine ((int) xe, r.getY(), r.getBottom()); g.fillEllipse (xe - 5, r.getBottom() - 10, 10, 10);
        if (e < s) { g.setFont (font (10, true)); g.drawText ("REVERSE", getLocalBounds().reduced (4), juce::Justification::topRight); }
    }

    void mouseDown (const juce::MouseEvent& m) override
    {
        const float x = (float) m.x / std::max (1, getWidth());
        // marcatore START in alto, END in basso
        dragging = m.y < getHeight() / 2 ? 0 : 1;
        if (std::abs (x - start()) < 0.03f && m.y < getHeight() / 2) dragging = 0;
        else if (std::abs (x - end()) < 0.03f && m.y >= getHeight() / 2) dragging = 1;
        setMarker (x);
    }
    void mouseDrag (const juce::MouseEvent& m) override { setMarker ((float) m.x / std::max (1, getWidth())); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".ogg") || f.endsWithIgnoreCase (".mp3")) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (ctx.proc.loadSample (ctx.voice, juce::File (files[0]))) { refresh(); if (onSampleChanged) onSampleChanged(); }
    }

private:
    Context& ctx; std::shared_ptr<const mp::SampleData> data; std::vector<float> peaks; int dragging = 0;
    float start() { return ctx.proc.param (ctx.voice, "start")->getValue(); }
    float end() { return ctx.proc.param (ctx.voice, "end")->getValue(); }
    void setMarker (float x)
    {
        auto* p = ctx.proc.param (ctx.voice, dragging == 0 ? "start" : "end");
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, x));
        repaint();
    }
};

// Visualizzatore degli ultimi 5 valori (RANDOM / VELOCITY)
class HistoryView : public juce::Component
{
public:
    HistoryView (Context& c, bool vel) : ctx (c), velocity (vel) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2);
        g.setColour (col::bg); g.fillRoundedRectangle (r, 3);
        auto& sh = ctx.shared();
        const int hp = sh.histPos.load();
        const float bw = r.getWidth() / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const int idx = (hp - 4 + i + 10) % 5;
            const float v = velocity ? sh.lastVelocity[idx].load() : sh.lastRandom[idx].load();
            auto b = juce::Rectangle<float> (r.getX() + i * bw + 2, r.getBottom() - v * (r.getHeight() - 4) - 2, bw - 4, v * (r.getHeight() - 4));
            g.setColour (i == 4 ? col::accent2 : col::accent2.withAlpha (0.4f)); g.fillRect (b);
        }
    }
private:
    Context& ctx; bool velocity;
};

class VoicePage : public juce::Component, private juce::Timer
{
public:
    explicit VoicePage (Context& c) : ctx (c), wave (c), randomView (c, false), velView (c, true),
        synthOn (c, "SYNTH"), sampOn (c, "SAMPLER"), env2Btn (c, "ENV 2"), stereoBtn (c, "STEREO"), postBtn (c, "POST"),
        engine (c), sampEng (c), sampEnv (c), fmode (c), dmode (c),
        l1sync (c, "SYNC"), l1bip (c, "BIPOLAR"), l2sync (c, "SYNC"), l2bip (c, "BIPOLAR"), l1wave (c), l1reset (c), l2wave (c), l2reset (c)
    {
        for (auto* k : { &pitch, &p1, &p2, &p3, &p4, &spitch, &sp1, &sp2, &sp3, &sp4, &blend, &cutoff, &res, &distort,
                         &macro1, &macro2, &e1a, &e1h, &e1d, &e1c, &e1v, &e2a, &e2h, &e2d, &e2c, &e2v,
                         &l1spd, &l1shape, &l1amp, &l2spd, &l2shape, &l2amp, &variation, &dsend, &rsend, &pan, &gain })
        { allKnobs.push_back (k); addAndMakeVisible (*k); }
        for (auto* b : { &synthOn, &sampOn, &env2Btn, &stereoBtn, &postBtn, &l1sync, &l1bip, &l2sync, &l2bip }) addAndMakeVisible (*b);
        for (auto* cb : { &engine, &sampEng, &sampEnv, &fmode, &dmode, &l1wave, &l1reset, &l2wave, &l2reset }) addAndMakeVisible (*cb);
        for (int s = 0; s < mp::kNumSources; ++s) { maps.push_back (std::make_unique<MapButton> (ctx, s)); addAndMakeVisible (*maps.back()); }
        addAndMakeVisible (wave); addAndMakeVisible (randomView); addAndMakeVisible (velView);
        wave.onSampleChanged = [this] { updateSampleName(); };

        for (auto* b : { &loadBtn, &prevBtn, &nextBtn, &rndSmpBtn, &clearBtn, &presetSave, &presetLoad, &voiceReset, &voiceDice })
            addAndMakeVisible (*b);
        addAndMakeVisible (sampleName);
        sampleName.setFont (font (12)); sampleName.setColour (juce::Label::textColourId, col::text);
        loadBtn.onClick = [this] { chooseSample(); };
        prevBtn.onClick = [this] { stepSample (-1); };
        nextBtn.onClick = [this] { stepSample (1); };
        rndSmpBtn.onClick = [this] { stepSample (0); };
        clearBtn.onClick = [this] { ctx.proc.clearSample (ctx.voice); wave.refresh(); updateSampleName(); };
        presetSave.onClick = [this] { savePreset(); };
        presetLoad.onClick = [this] { loadPreset(); };
        voiceReset.onClick = [this] { ctx.proc.resetVoice (ctx.voice); ctx.proc.clearSample (ctx.voice); wave.refresh(); updateSampleName(); };
        voiceDice.onClick = [this] { ctx.proc.randomizeVoice (ctx.voice); };
        voiceReset.setTooltip ("Riporta la voce ai valori predefiniti");
        voiceDice.setTooltip ("Randomizza la voce (profondità nelle Opzioni)");
        blend.setTitle ("BLEND");
        auto lfoText = [this] (const char* syncId) { return [this, syncId] (float n) {
            const bool sync = ctx.proc.param (ctx.voice, syncId)->getValue() > 0.5f;
            if (sync) return juce::String (mp::lfoDivName (mp::lfoDivIndex (n)));
            const float hz = mp::lfoHz (n); return hz < 10 ? juce::String (hz, 2) + " Hz" : juce::String (hz, 1) + " Hz"; }; };
        l1spd.textFn = lfoText ("l1sync"); l2spd.textFn = lfoText ("l2sync");
        engine.onChange = [this] { updateEngineLabels(); };
        sampEng.onChange = [this] { updateEngineLabels(); };
        rebind();
        startTimerHz (25);
    }

    void rebind()
    {
        int i = 0;
        const int tgt[] = { mp::T_Pitch, mp::T_P1, mp::T_P2, mp::T_P3, mp::T_P4, mp::T_SPitch, mp::T_SP1, mp::T_SP2, mp::T_SP3, mp::T_SP4,
                            mp::T_Blend, mp::T_Cutoff, mp::T_Res, mp::T_Distort };
        for (auto* k : { &pitch, &p1, &p2, &p3, &p4, &spitch, &sp1, &sp2, &sp3, &sp4, &blend, &cutoff, &res, &distort })
        { k->bind (mpp::targetId (tgt[i]), tgt[i]); ++i; }
        for (auto* k : { &sp1, &sp2, &sp3, &sp4 }) k->setBipolar (true);
        macro1.bind ("macro1"); macro2.bind ("macro2");
        e1a.bind ("e1a", mp::T_E1Attack); e1h.bind ("e1h", mp::T_E1Hold); e1d.bind ("e1d", mp::T_E1Decay); e1c.bind ("e1curve"); e1v.bind ("e1vel");
        e2a.bind ("e2a", mp::T_E2Attack); e2h.bind ("e2h", mp::T_E2Hold); e2d.bind ("e2d", mp::T_E2Decay); e2c.bind ("e2curve"); e2v.bind ("e2vel");
        l1spd.bind ("l1spd", mp::T_L1Speed); l1amp.bind ("l1amp", mp::T_L1Amp); l1shape.bind ("l1shape");
        l2spd.bind ("l2spd", mp::T_L2Speed); l2amp.bind ("l2amp", mp::T_L2Amp); l2shape.bind ("l2shape");
        variation.bind ("variation"); dsend.bind ("dsend", mp::T_DelaySend); rsend.bind ("rsend", mp::T_RevSend);
        pan.bind ("pan", mp::T_Pan); gain.bind ("gain", mp::T_Gain); pan.setBipolar (true);
        synthOn.bind ("synthOn"); sampOn.bind ("sampOn"); env2Btn.bind ("env2"); stereoBtn.bind ("stereo"); postBtn.bind ("post");
        l1sync.bind ("l1sync"); l1bip.bind ("l1bip"); l2sync.bind ("l2sync"); l2bip.bind ("l2bip");
        engine.bind ("engine"); sampEng.bind ("sampEng"); sampEnv.bind ("sampEnv"); fmode.bind ("fmode"); dmode.bind ("dmode");
        l1wave.bind ("l1wave"); l1reset.bind ("l1reset"); l2wave.bind ("l2wave"); l2reset.bind ("l2reset");
        updateEngineLabels();
        wave.refresh(); updateSampleName();
        repaint();
    }

    void armedChanged() { for (auto* k : allKnobs) k->repaint(); for (auto& m : maps) m->repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (auto& p : panels) drawPanel (g, p.first, p.second);
        g.setColour (col::dim); g.setFont (font (10.5f, true));
        for (auto* v : { (juce::Component*) &randomView, (juce::Component*) &velView })
            g.drawText (v == &randomView ? "RANDOM" : "VELOCITY", v->getX(), v->getBottom() + 1, v->getWidth(), 13, juce::Justification::centred);
    }

    void resized() override
    {
        panels.clear();
        auto r = getLocalBounds().reduced (6);
        const int gap = 6;
        auto top = r.removeFromTop (262); r.removeFromTop (gap);
        auto mid = r.removeFromTop (150); r.removeFromTop (gap);
        auto bot = r;

        // ---- SYNTH ----
        auto syn = top.removeFromLeft (300); top.removeFromLeft (gap);
        panels.push_back ({ syn, "" });
        { auto h = syn.reduced (8).removeFromTop (22); synthOn.setBounds (h.removeFromLeft (80)); stereoBtn.setBounds (h.removeFromRight (64)); h.removeFromRight (4); env2Btn.setBounds (h.removeFromRight (56)); }
        engine.setBounds (syn.getX() + 8, syn.getY() + 36, syn.getWidth() - 16, 24);
        layoutRow ({ &pitch, &p1, &p2, &p3, &p4 }, { syn.getX() + 4, syn.getY() + 70, syn.getWidth() - 8, 84 });
        // ---- SAMPLER ----
        auto smp = top.removeFromLeft (330); top.removeFromLeft (gap);
        panels.push_back ({ smp, "" });
        {
            auto h = smp.reduced (8).removeFromTop (22);
            sampOn.setBounds (h.removeFromLeft (80)); h.removeFromLeft (6);
            sampEnv.setBounds (h.removeFromRight (78)); h.removeFromRight (4);
            sampEng.setBounds (h);
        }
        wave.setBounds (smp.getX() + 8, smp.getY() + 36, smp.getWidth() - 16, 92);
        {
            juce::Rectangle<int> row (smp.getX() + 8, smp.getY() + 132, smp.getWidth() - 16, 22);
            loadBtn.setBounds (row.removeFromLeft (58)); row.removeFromLeft (4);
            clearBtn.setBounds (row.removeFromRight (24)); row.removeFromRight (3);
            rndSmpBtn.setBounds (row.removeFromRight (34)); row.removeFromRight (3);
            nextBtn.setBounds (row.removeFromRight (24)); row.removeFromRight (2);
            prevBtn.setBounds (row.removeFromRight (24)); row.removeFromRight (4);
            sampleName.setBounds (row);
        }
        layoutRow ({ &spitch, &sp1, &sp2, &sp3, &sp4 }, { smp.getX() + 4, smp.getY() + 160, smp.getWidth() - 8, 84 });
        // ---- BLEND (verticale) ----
        auto bl = top.removeFromLeft (60); top.removeFromLeft (gap);
        panels.push_back ({ bl, "" });
        blend.setBounds (bl.reduced (4, 60));
        // ---- TONE ----
        auto tone = top;
        panels.push_back ({ tone, "TONE" });
        {
            auto in = tone.reduced (8); in.removeFromTop (22);
            auto knobs = in.removeFromTop (110);
            layoutRow ({ &cutoff, &res, &distort }, knobs);
            auto row = in.removeFromTop (24);
            fmode.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
            postBtn.setBounds (row.removeFromLeft (46).reduced (2, 0));
            dmode.setBounds (row.reduced (2, 0));
            in.removeFromTop (8);
            auto pr = in.removeFromTop (24);
            presetLoad.setBounds (pr.removeFromLeft (pr.getWidth() / 2).reduced (2, 0)); presetSave.setBounds (pr.reduced (2, 0));
            in.removeFromTop (4);
            auto pr2 = in.removeFromTop (24);
            voiceReset.setBounds (pr2.removeFromLeft (pr2.getWidth() / 2).reduced (2, 0)); voiceDice.setBounds (pr2.reduced (2, 0));
        }

        // ---- MODULATION ----
        auto mod = mid.removeFromLeft (360); mid.removeFromLeft (gap);
        panels.push_back ({ mod, "MODULATION" });
        {
            auto in = mod.reduced (8); in.removeFromTop (24);
            const int w = in.getWidth() / 4;
            auto c0 = in.removeFromLeft (w), c1 = in.removeFromLeft (w), c2 = in.removeFromLeft (w), c3 = in;
            maps[mp::S_Random]->setBounds (c0.removeFromTop (20).reduced (12, 0)); randomView.setBounds (c0.reduced (2, 4).withTrimmedBottom (12));
            maps[mp::S_Velocity]->setBounds (c1.removeFromTop (20).reduced (12, 0)); velView.setBounds (c1.reduced (2, 4).withTrimmedBottom (12));
            maps[mp::S_Macro1]->setBounds (c2.removeFromTop (20).reduced (12, 0)); macro1.setBounds (c2);
            maps[mp::S_Macro2]->setBounds (c3.removeFromTop (20).reduced (12, 0)); macro2.setBounds (c3);
        }
        auto env1 = mid.removeFromLeft ((mid.getWidth() - gap) / 2); mid.removeFromLeft (gap);
        auto env2 = mid;
        panels.push_back ({ env1, "ENVELOPE 1" }); panels.push_back ({ env2, "ENVELOPE 2" });
        maps[mp::S_Env1]->setBounds (env1.getRight() - 50, env1.getY() + 4, 40, 18);
        maps[mp::S_Env2]->setBounds (env2.getRight() - 50, env2.getY() + 4, 40, 18);
        layoutRow ({ &e1a, &e1h, &e1d, &e1c, &e1v }, env1.reduced (4).withTrimmedTop (30));
        layoutRow ({ &e2a, &e2h, &e2d, &e2c, &e2v }, env2.reduced (4).withTrimmedTop (30));

        // ---- LFO + VARIATION/MIX ----
        auto lfo1 = bot.removeFromLeft (300); bot.removeFromLeft (gap);
        auto lfo2 = bot.removeFromLeft (300); bot.removeFromLeft (gap);
        auto mix = bot;
        panels.push_back ({ lfo1, "LFO 1" }); panels.push_back ({ lfo2, "LFO 2" }); panels.push_back ({ mix, "VARIATION / MIX" });
        maps[mp::S_Lfo1]->setBounds (lfo1.getRight() - 50, lfo1.getY() + 4, 40, 18);
        maps[mp::S_Lfo2]->setBounds (lfo2.getRight() - 50, lfo2.getY() + 4, 40, 18);
        layoutLfo (lfo1, l1spd, l1shape, l1amp, l1sync, l1bip, l1wave, l1reset);
        layoutLfo (lfo2, l2spd, l2shape, l2amp, l2sync, l2bip, l2wave, l2reset);
        {
            auto in = mix.reduced (6); in.removeFromTop (24);
            layoutRow ({ &variation, &dsend, &rsend, &pan, &gain }, in.withSizeKeepingCentre (in.getWidth(), std::min (in.getHeight(), 120)));
        }
    }

private:
    Context& ctx;
    WaveView wave; HistoryView randomView, velView;
    ParamKnob pitch { ctx, "PITCH" }, p1 { ctx }, p2 { ctx }, p3 { ctx }, p4 { ctx },
              spitch { ctx, "PITCH" }, sp1 { ctx }, sp2 { ctx }, sp3 { ctx }, sp4 { ctx },
              blend { ctx, "BLEND" }, cutoff { ctx, "CUTOFF" }, res { ctx, "RESONANCE" }, distort { ctx, "DISTORT" },
              macro1 { ctx, "MACRO 1" }, macro2 { ctx, "MACRO 2" },
              e1a { ctx, "ATTACK" }, e1h { ctx, "HOLD" }, e1d { ctx, "DECAY" }, e1c { ctx, "CURVE" }, e1v { ctx, "VELOCITY" },
              e2a { ctx, "ATTACK" }, e2h { ctx, "HOLD" }, e2d { ctx, "DECAY" }, e2c { ctx, "CURVE" }, e2v { ctx, "VELOCITY" },
              l1spd { ctx, "SPEED" }, l1shape { ctx, "SHAPE" }, l1amp { ctx, "AMP" },
              l2spd { ctx, "SPEED" }, l2shape { ctx, "SHAPE" }, l2amp { ctx, "AMP" },
              variation { ctx, "VARIATION" }, dsend { ctx, "DELAY" }, rsend { ctx, "REVERB" }, pan { ctx, "PAN" }, gain { ctx, "VOICE GAIN" };
    ParamToggle synthOn, sampOn, env2Btn, stereoBtn, postBtn;
    ParamCombo engine, sampEng, sampEnv, fmode, dmode;
    ParamToggle l1sync, l1bip, l2sync, l2bip;
    ParamCombo l1wave, l1reset, l2wave, l2reset;
    std::vector<std::unique_ptr<MapButton>> maps;
    std::vector<ParamKnob*> allKnobs;
    juce::TextButton loadBtn { "CARICA" }, prevBtn { "<" }, nextBtn { ">" }, rndSmpBtn { "RND" }, clearBtn { "X" },
                     presetSave { "SALVA VOCE" }, presetLoad { "CARICA VOCE" }, voiceReset { "RESET" }, voiceDice { "DADO" };
    juce::Label sampleName;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
    std::unique_ptr<juce::FileChooser> chooser;

    void timerCallback() override
    {
        // aggiorna gli indicatori di modulazione e la cronologia
        for (auto* k : allKnobs) k->getKnob().repaint();
        randomView.repaint(); velView.repaint(); wave.repaint();
    }

    void layoutRow (std::initializer_list<juce::Component*> comps, juce::Rectangle<int> area)
    {
        const int n = (int) comps.size(); const int w = area.getWidth() / n;
        int i = 0;
        for (auto* c : comps) { c->setBounds (area.getX() + i * w, area.getY(), w, area.getHeight()); ++i; }
    }
    void layoutLfo (juce::Rectangle<int> r, ParamKnob& spd, ParamKnob& shape, ParamKnob& amp, ParamToggle& sync, ParamToggle& bip, ParamCombo& wv, ParamCombo& rs)
    {
        auto in = r.reduced (6); in.removeFromTop (24);
        auto bottom = in.removeFromBottom (24);
        wv.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0)); rs.setBounds (bottom.reduced (2, 0));
        in.removeFromBottom (4);
        auto btns = in.removeFromRight (72).withSizeKeepingCentre (72, 58);
        sync.setBounds (btns.removeFromTop (26).reduced (2, 0)); btns.removeFromTop (6); bip.setBounds (btns.removeFromTop (26).reduced (2, 0));
        layoutRow ({ &spd, &shape, &amp }, in);
    }

    void updateEngineLabels()
    {
        const auto& info = mp::engineInfo (juce::jmax (0, engine.getSelectedItemIndex()));
        p1.setTitle (info.k[0]); p2.setTitle (info.k[1]); p3.setTitle (info.k[2]); p4.setTitle (info.k[3]);
        const auto& si = mp::samplerInfo (juce::jmax (0, sampEng.getSelectedItemIndex()));
        sp1.setTitle (si.k[0]); sp2.setTitle (si.k[1]); sp3.setTitle (si.k[2]); sp4.setTitle (si.k[3]);
    }
    void updateSampleName()
    {
        auto f = ctx.proc.getSampleFile (ctx.voice);
        sampleName.setText (f.existsAsFile() ? f.getFileName() : juce::String ("(nessun campione)"), juce::dontSendNotification);
    }
    void chooseSample()
    {
        auto start = ctx.proc.getSampleFile (ctx.voice);
        chooser = std::make_unique<juce::FileChooser> ("Carica campione", start.existsAsFile() ? start.getParentDirectory() : ManipoloProcessor::userFolder(),
                                                        ctx.proc.formats.getWildcardForAllFormats());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile() && ctx.proc.loadSample (ctx.voice, fc.getResult())) { wave.refresh(); updateSampleName(); }
        });
    }
    void stepSample (int dir)
    {
        auto cur = ctx.proc.getSampleFile (ctx.voice);
        juce::File folder = cur.existsAsFile() ? cur.getParentDirectory() : ManipoloProcessor::userFolder().getChildFile ("Samples");
        juce::Array<juce::File> files;
        for (auto& f : folder.findChildFiles (juce::File::findFiles, dir == 0, "*.wav;*.aif;*.aiff;*.flac"))
            files.add (f);
        if (files.isEmpty()) return;
        files.sort();
        int idx = files.indexOf (cur);
        if (dir == 0) idx = juce::Random::getSystemRandom().nextInt (files.size());
        else idx = (idx + dir + files.size()) % files.size();
        if (ctx.proc.loadSample (ctx.voice, files[idx])) { wave.refresh(); updateSampleName(); }
    }
    void savePreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Salva preset voce", ManipoloProcessor::userFolder().getChildFile ("Voices"), "*.mvoice");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File()) ctx.proc.saveVoicePreset (ctx.voice, f.withFileExtension ("mvoice"));
        });
    }
    void loadPreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Carica preset voce", ManipoloProcessor::userFolder().getChildFile ("Voices"), "*.mvoice");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) { ctx.proc.loadVoicePreset (ctx.voice, fc.getResult()); wave.refresh(); updateSampleName(); }
        });
    }
};
} // namespace ui
