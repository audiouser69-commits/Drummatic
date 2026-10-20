#pragma once
#include "Widgets.h"

namespace ui
{
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (Context& c, int v) : ctx (c), voice (v), choke (c), link (c), route (c)
    {
        for (auto* k : { &dly, &rev, &pan }) addAndMakeVisible (*k);
        addAndMakeVisible (gainSlider); addAndMakeVisible (choke); addAndMakeVisible (link); addAndMakeVisible (route);
        route.setTooltip ("Uscita: Main 1-2 o uscite ausiliarie (attivale nel DAW). Il MASTER agisce solo sul Main");
        addAndMakeVisible (muteBtn); addAndMakeVisible (soloBtn);
        gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 64, 18);
        gainSlider.setColour (juce::Slider::trackColourId, col::accent);
        gainSlider.setColour (juce::Slider::backgroundColourId, col::bg);
        gainSlider.setColour (juce::Slider::thumbColourId, col::text);
        gainSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        muteBtn.setClickingTogglesState (true); soloBtn.setClickingTogglesState (true);
        muteBtn.setColour (juce::TextButton::buttonOnColourId, col::warn);
        soloBtn.setColour (juce::TextButton::buttonOnColourId, col::accent2);
        muteBtn.onClick = [this] { ctx.proc.dm.mute[voice] = muteBtn.getToggleState(); };
        soloBtn.onClick = [this] { ctx.proc.dm.solo[voice] = soloBtn.getToggleState(); };
        rebind();
    }
    void rebind()
    {
        dly.bind (mpp::vid (voice, "dsend"), -1, false);
        rev.bind (mpp::vid (voice, "rsend"), -1, false);
        pan.bind (mpp::vid (voice, "pan"), -1, false); pan.setBipolar (true);
        gainAtt = std::make_unique<juce::SliderParameterAttachment> (*ctx.proc.param (voice, "gain"), gainSlider, nullptr);
        choke.bind (mpp::vid (voice, "choke"), false); link.bind (mpp::vid (voice, "link"), false); route.bind (mpp::vid (voice, "route"), false);
    }
    void sync()
    {
        muteBtn.setToggleState (ctx.proc.dm.mute[voice].load(), juce::dontSendNotification);
        soloBtn.setToggleState (ctx.proc.dm.solo[voice].load(), juce::dontSendNotification);
        repaint (0, 0, 40, getHeight());
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (voice == ctx.voice ? col::panel2 : col::panel); g.fillRoundedRectangle (r, 4);
        g.setColour (col::accent); g.setFont (font (18, true));
        g.drawText (juce::String (voice + 1), 6, 0, 26, getHeight(), juce::Justification::centred);
        const float lvl = juce::jlimit (0.0f, 1.0f, ctx.proc.dm.vshared[voice].level.load());
        g.setColour (col::bg); g.fillRect (34.0f, 6.0f, 5.0f, r.getHeight() - 12);
        g.setColour (lvl > 0.95f ? col::warn : col::accent2);
        const float h = (r.getHeight() - 12) * std::sqrt (lvl);
        g.fillRect (34.0f, r.getHeight() - 6 - h, 5.0f, h);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (4); r.removeFromLeft (40);
        dly.setBounds (r.removeFromLeft (64)); rev.setBounds (r.removeFromLeft (64)); pan.setBounds (r.removeFromLeft (64));
        r.removeFromLeft (6);
        auto ms = r.removeFromRight (64);
        muteBtn.setBounds (ms.removeFromLeft (30).withSizeKeepingCentre (28, 24)); soloBtn.setBounds (ms.withSizeKeepingCentre (28, 24));
        auto cmb = r.removeFromRight (240);
        auto c1 = cmb.removeFromLeft (70), c2 = cmb.removeFromLeft (70), c3 = cmb;
        link.setBounds (c1.withSizeKeepingCentre (66, 24)); choke.setBounds (c2.withSizeKeepingCentre (66, 24)); route.setBounds (c3.withSizeKeepingCentre (94, 24));
        gainSlider.setBounds (r.withSizeKeepingCentre (r.getWidth() - 8, 28));
    }
private:
    Context& ctx; int voice;
    ParamKnob dly { ctx, "DELAY" }, rev { ctx, "REVERB" }, pan { ctx, "PAN" };
    juce::Slider gainSlider; std::unique_ptr<juce::SliderParameterAttachment> gainAtt;
    ParamCombo choke, link, route;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
};

class MixerPage : public juce::Component, private juce::Timer
{
public:
    explicit MixerPage (Context& c) : ctx (c), dSync (c, "SYNC"), dStereo (c, "STEREO"), dPong (c, "PONG"), rSync (c, "SYNC"),
                                     dDiv (c), rDiv (c), rMode (c)
    {
        for (int v = 0; v < mp::kNumVoices; ++v) { strips.add (new ChannelStrip (ctx, v)); addAndMakeVisible (strips.getLast()); }
        for (auto* k : { &dTime, &dFb, &dGlitch, &dDuck, &dTilt, &dSRate, &dGain, &toRev, &rPre, &rDecay, &rSRate, &rGain, &rIn, &rOut })
            addAndMakeVisible (*k);
        for (auto* k : { &dTilt, &rIn, &rOut }) k->setBipolar (true);
        for (auto* b : { &dSync, &dStereo, &dPong, &rSync }) addAndMakeVisible (*b);
        for (auto* cb : { &dDiv, &rDiv, &rMode }) addAndMakeVisible (*cb);
        dTime.bind ("d_time", -1, false); dFb.bind ("d_fb", -1, false); dGlitch.bind ("d_glitch", -1, false); dDuck.bind ("d_duck", -1, false);
        dTilt.bind ("d_fbtilt", -1, false); dSRate.bind ("d_srate", -1, false); dGain.bind ("d_gain", -1, false); toRev.bind ("d_torev", -1, false);
        rPre.bind ("r_pre", -1, false); rDecay.bind ("r_decay", -1, false); rSRate.bind ("r_srate", -1, false); rGain.bind ("r_gain", -1, false);
        rIn.bind ("r_intilt", -1, false); rOut.bind ("r_outtilt", -1, false);
        dSync.bind ("d_sync", false); dStereo.bind ("d_stereo", false); dPong.bind ("d_pong", false); rSync.bind ("r_sync", false);
        dDiv.bind ("d_div", false); rDiv.bind ("r_div", false); rMode.bind ("r_mode", false);
        startTimerHz (20);
    }
    void rebind() { for (auto* s : strips) s->rebind(); }

    void paint (juce::Graphics& g) override
    {
        for (auto& p : panels) drawPanel (g, p.first, p.second);
        g.setColour (col::dim); g.setFont (font (10.5f, true));
        auto h = header;
        h.removeFromLeft (44);
        for (auto* t : { "DELAY", "REVERB", "PAN" }) g.drawText (t, h.removeFromLeft (64), juce::Justification::centred);
        auto rr = h.removeFromRight (64 + 240);
        g.drawText ("TRIGGER", rr.removeFromLeft (70), juce::Justification::centred);
        g.drawText ("CHOKE", rr.removeFromLeft (70), juce::Justification::centred);
        g.drawText ("ROUTING", rr.removeFromLeft (100), juce::Justification::centred);
        g.drawText ("GAIN", h, juce::Justification::centred);
    }
    void resized() override
    {
        panels.clear();
        auto r = getLocalBounds().reduced (6);
        auto fx = r.removeFromRight (300); r.removeFromRight (6);
        header = r.removeFromTop (18);
        const int h = r.getHeight() / mp::kNumVoices;
        for (auto* s : strips) s->setBounds (r.removeFromTop (h).reduced (0, 2));

        auto del = fx.removeFromTop (fx.getHeight() / 2 + 20); fx.removeFromTop (6);
        auto rev = fx;
        panels.push_back ({ del, "SHATTER DELAY" }); panels.push_back ({ rev, "HEADSPACE REVERB" });
        {
            auto in = del.reduced (6).withTrimmedTop (24);
            auto row1 = in.removeFromTop (96); in.removeFromTop (6); auto row2 = in.removeFromTop (96); in.removeFromTop (6);
            const int w = row1.getWidth() / 4;
            dTime.setBounds (row1.removeFromLeft (w)); dFb.setBounds (row1.removeFromLeft (w)); dGlitch.setBounds (row1.removeFromLeft (w)); dDuck.setBounds (row1);
            dTilt.setBounds (row2.removeFromLeft (w)); dSRate.setBounds (row2.removeFromLeft (w)); dGain.setBounds (row2.removeFromLeft (w)); toRev.setBounds (row2);
            auto b = in.removeFromTop (26);
            const int bw = b.getWidth() / 4;
            dSync.setBounds (b.removeFromLeft (bw).reduced (2)); dDiv.setBounds (b.removeFromLeft (bw).reduced (2));
            dStereo.setBounds (b.removeFromLeft (bw).reduced (2)); dPong.setBounds (b.reduced (2));
        }
        {
            auto in = rev.reduced (6).withTrimmedTop (24);
            auto row1 = in.removeFromTop (96); in.removeFromTop (6); auto row2 = in.removeFromTop (96); in.removeFromTop (6);
            const int w = row1.getWidth() / 4;
            rPre.setBounds (row1.removeFromLeft (w)); rDecay.setBounds (row1.removeFromLeft (w)); rSRate.setBounds (row1.removeFromLeft (w)); rGain.setBounds (row1);
            rIn.setBounds (row2.removeFromLeft (w)); rOut.setBounds (row2.removeFromLeft (w));
            auto b = in.removeFromTop (26);
            rMode.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (2)); rSync.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (2)); rDiv.setBounds (b.reduced (2));
        }
    }
private:
    Context& ctx;
    juce::OwnedArray<ChannelStrip> strips;
    ParamKnob dTime { ctx, "TIME" }, dFb { ctx, "FEEDBACK" }, dGlitch { ctx, "GLITCH" }, dDuck { ctx, "DUCKING" },
              dTilt { ctx, "FB TILT" }, dSRate { ctx, "S.RATE" }, dGain { ctx, "GAIN" }, toRev { ctx, "> REVERB" },
              rPre { ctx, "PREDELAY" }, rDecay { ctx, "DECAY" }, rSRate { ctx, "S.RATE" }, rGain { ctx, "GAIN" },
              rIn { ctx, "IN TILT" }, rOut { ctx, "OUT TILT" };
    ParamToggle dSync, dStereo, dPong, rSync;
    ParamCombo dDiv, rDiv, rMode;
    juce::Rectangle<int> header;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> panels;
    void timerCallback() override
    {
        for (auto* s : strips) s->sync();
        const bool ds = ctx.proc.apvts.getRawParameterValue ("d_sync")->load() > 0.5f;
        dTime.setEnabled (! ds); dDiv.setEnabled (ds); dTime.setAlpha (ds ? 0.35f : 1.0f);
        const bool rs = ctx.proc.apvts.getRawParameterValue ("r_sync")->load() > 0.5f;
        rPre.setEnabled (! rs); rDiv.setEnabled (rs); rPre.setAlpha (rs ? 0.35f : 1.0f);
    }
};
} // namespace ui
