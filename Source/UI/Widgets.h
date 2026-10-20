#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace ui
{
// Stato condiviso dell'interfaccia
struct Context
{
    ManipoloProcessor& proc;
    int voice = 0;
    int armedSource = -1;                 // sorgente di modulazione in modalità MAP (-1 = nessuna)
    std::function<void()> onArmedChanged;
    explicit Context (ManipoloProcessor& p) : proc (p) {}
    mp::VoiceShared& shared() { return proc.dm.vshared[voice]; }
};

// ---------------- Manopola ----------------
class Knob : public juce::Slider
{
public:
    Knob (Context& c) : ctx (c)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (180);
        setVelocityBasedMode (false);
    }
    int target = -1;            // indice mp::Target se modulabile
    std::atomic<bool>* lockFlag = nullptr;   // lucchetto alternativo (es. controlli Performance)
    bool bipolarArc = false;
    bool perVoice = true;

    bool modMode() const { return target >= 0 && ctx.armedSource >= 0; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const float size = std::min (r.getWidth(), r.getHeight());
        auto k = r.withSizeKeepingCentre (size, size);
        const auto rp = getRotaryParameters();
        const float a0 = rp.startAngleRadians, a1 = rp.endAngleRadians;
        const float norm = (float) valueToProportionOfLength (getValue());
        const float cx = k.getCentreX(), cy = k.getCentreY(), rad = size * 0.5f;

        if (modMode()) { g.setColour (col::accent2.withAlpha (0.12f)); g.fillEllipse (k.expanded (2)); }
        g.setColour (col::knob); g.fillEllipse (k.reduced (size * 0.16f));
        // traccia
        juce::Path track; track.addCentredArc (cx, cy, rad - 2, rad - 2, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // valore
        const float from = bipolarArc ? (a0 + a1) * 0.5f : a0;
        const float to = a0 + (a1 - a0) * norm;
        juce::Path val; val.addCentredArc (cx, cy, rad - 2, rad - 2, 0, std::min (from, to), std::max (from, to), true);
        g.setColour (col::accent); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (target >= 0)
        {
            auto& sh = ctx.shared();
            // intervallo di modulazione della sorgente armata (o somma delle profondità)
            float depth = 0;
            if (ctx.armedSource >= 0) depth = sh.depth[ctx.armedSource][target].load();
            if (depth != 0.0f)
            {
                const float end = juce::jlimit (0.0f, 1.0f, norm + depth);
                juce::Path m; m.addCentredArc (cx, cy, rad + 1.5f, rad + 1.5f, 0, a0 + (a1 - a0) * std::min (norm, end), a0 + (a1 - a0) * std::max (norm, end), true);
                g.setColour (col::accent2); g.strokePath (m, juce::PathStrokeType (2.0f));
            }
            // valore modulato attuale (puntino)
            const float disp = sh.display[target].load();
            bool anyMod = false;
            for (int src = 0; src < mp::kNumSources; ++src) anyMod |= sh.depth[src][target].load() != 0.0f;
            if (anyMod && std::abs (disp - norm) > 0.004f)
            {
                const float ang = a0 + (a1 - a0) * disp;
                g.setColour (col::accent2);
                g.fillEllipse (cx + std::sin (ang) * (rad - 2) - 2.5f, cy - std::cos (ang) * (rad - 2) - 2.5f, 5, 5);
            }
            if (sh.locked[target].load())
            {
                g.setColour (col::warn); g.setFont (font (9.0f, true));
                g.drawText ("L", (int) (k.getRight() - 9), (int) k.getY(), 10, 10, juce::Justification::centred);
            }
        }
        if (lockFlag != nullptr && lockFlag->load())
        {
            g.setColour (col::warn); g.setFont (font (9.0f, true));
            g.drawText ("L", (int) (k.getRight() - 9), (int) k.getY(), 10, 10, juce::Justification::centred);
        }
        // indicatore
        const float ang = modMode() ? a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, 0.5f + ctx.shared().depth[ctx.armedSource][target].load() * 0.5f) : to;
        g.setColour (modMode() ? col::accent2 : col::text);
        juce::Line<float> ln (cx + std::sin (ang) * rad * 0.2f, cy - std::cos (ang) * rad * 0.2f, cx + std::sin (ang) * (rad * 0.62f), cy - std::cos (ang) * (rad * 0.62f));
        g.drawLine (ln, 2.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && lockFlag != nullptr) { *lockFlag = ! lockFlag->load(); repaint(); return; }
        if (e.mods.isPopupMenu() && target >= 0)
        {
            auto& l = ctx.shared().locked[target];
            l = ! l.load(); repaint(); return;
        }
        if (modMode()) { dragStartDepth = ctx.shared().depth[ctx.armedSource][target].load(); return; }
        if (e.mods.isAltDown()) { if (onReset) onReset(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (modMode())
        {
            const float d = juce::jlimit (-1.0f, 1.0f, dragStartDepth + (float) (-e.getDistanceFromDragStartY() + e.getDistanceFromDragStartX()) / 150.0f);
            ctx.shared().depth[ctx.armedSource][target] = std::abs (d) < 0.01f ? 0.0f : d;
            repaint(); return;
        }
        juce::Slider::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! modMode() && ! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (modMode()) { ctx.shared().depth[ctx.armedSource][target] = 0.0f; repaint(); return; }
        juce::Slider::mouseDoubleClick (e);
    }
    std::function<void()> onReset;

private:
    Context& ctx;
    float dragStartDepth = 0;
};

// Manopola con titolo, valore e binding al parametro
class ParamKnob : public juce::Component
{
public:
    ParamKnob (Context& c, const juce::String& t = {}) : ctx (c), knob (c), title (t)
    {
        addAndMakeVisible (knob);
        knob.onValueChange = [this] { repaint(); };
        knob.onReset = [this] { if (param) param->setValueNotifyingHost (param->getDefaultValue()); };
    }
    // nome = parametro per-voce (es. "cutoff") o globale se perVoice=false
    void bind (const juce::String& name, int target = -1, bool perVoice = true)
    {
        att.reset();
        paramName = name; knob.target = target; knob.perVoice = perVoice;
        param = perVoice ? ctx.proc.param (ctx.voice, name) : ctx.proc.apvts.getParameter (name);
        if (param != nullptr)
        {
            att = std::make_unique<juce::SliderParameterAttachment> (*param, knob, nullptr);
            knob.setTooltip (param->getName (64) + (target >= 0 ? "  (tasto destro: blocca, MAP: modulazione)" : ""));
        }
        repaint();
    }
    void rebind() { if (paramName.isNotEmpty()) bind (paramName, knob.target, knob.perVoice); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setBipolar (bool b) { knob.bipolarArc = b; }
    Knob& getKnob() { return knob; }
    std::function<juce::String (float)> textFn;   // testo personalizzato (valore normalizzato)

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (14); r.removeFromBottom (14);
        knob.setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::dim); g.setFont (font (10.5f, true));
        g.drawText (title, 0, 0, getWidth(), 13, juce::Justification::centred);
        g.setColour (knob.modMode() ? col::accent2 : col::text); g.setFont (font (11.0f));
        juce::String txt = param ? (textFn ? textFn (param->getValue()) : param->getCurrentValueAsText()) : juce::String();
        if (knob.modMode()) txt = juce::String (juce::roundToInt (ctx.shared().depth[ctx.armedSource][knob.target].load() * 100)) + " %";
        g.drawText (txt, 0, getHeight() - 13, getWidth(), 13, juce::Justification::centred);
    }

private:
    Context& ctx;
    Knob knob;
    juce::String title, paramName;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

// Pulsante on/off legato a un parametro bool
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (Context& c, const juce::String& text) : juce::TextButton (text), ctx (c) { setClickingTogglesState (true); }
    void bind (const juce::String& name, bool perVoice = true)
    {
        att.reset(); paramName = name; pv = perVoice;
        if (auto* p = perVoice ? ctx.proc.param (ctx.voice, name) : ctx.proc.apvts.getParameter (name))
            att = std::make_unique<juce::ButtonParameterAttachment> (*p, *this, nullptr);
    }
    void rebind() { if (paramName.isNotEmpty()) bind (paramName, pv); }
private:
    Context& ctx; juce::String paramName; bool pv = true;
    std::unique_ptr<juce::ButtonParameterAttachment> att;
};

// Menu a tendina legato a un parametro choice
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (Context& c) : ctx (c) {}
    void bind (const juce::String& name, bool perVoice = true)
    {
        att.reset(); paramName = name; pv = perVoice;
        clear (juce::dontSendNotification);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (perVoice ? ctx.proc.param (ctx.voice, name) : ctx.proc.apvts.getParameter (name)))
        {
            addItemList (p->choices, 1);
            att = std::make_unique<juce::ComboBoxParameterAttachment> (*p, *this, nullptr);
        }
    }
    void rebind() { if (paramName.isNotEmpty()) bind (paramName, pv); }
private:
    Context& ctx; juce::String paramName; bool pv = true;
    std::unique_ptr<juce::ComboBoxParameterAttachment> att;
};

// Pulsante MAP di modulazione
class MapButton : public juce::Button
{
public:
    MapButton (Context& c, int src) : juce::Button ("MAP"), ctx (c), source (src)
    {
        setTooltip (juce::String ("Modalità modulazione: ") + mp::sourceName (src) + " - trascina le manopole per impostare la profondità");
        onClick = [this]
        {
            ctx.armedSource = ctx.armedSource == source ? -1 : source;
            if (ctx.onArmedChanged) ctx.onArmedChanged();
        };
    }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = ctx.armedSource == source;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? col::accent2 : (over ? col::panel2.brighter (0.1f) : col::panel2)); g.fillRoundedRectangle (r, 3);
        g.setColour (on ? col::accent2.brighter() : col::line); g.drawRoundedRectangle (r, 3, 1);
        // icona: onda + manopola
        g.setColour (on ? juce::Colours::black : col::accent2);
        juce::Path w; const float y = r.getCentreY(), x0 = r.getX() + 5;
        w.startNewSubPath (x0, y);
        for (int i = 1; i <= 12; ++i) w.lineTo (x0 + i * 1.2f, y - std::sin (i * 0.9f) * 3.5f);
        g.strokePath (w, juce::PathStrokeType (1.4f));
        g.fillEllipse (r.getRight() - 13, y - 4, 8, 8);
    }
private:
    Context& ctx; int source;
};
} // namespace ui
