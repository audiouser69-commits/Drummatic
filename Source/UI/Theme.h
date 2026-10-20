#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
// Palette originale di Manipolo: grafite caldo, ambra, menta
namespace col
{
    const juce::Colour bg       { 0xff131518 };
    const juce::Colour panel    { 0xff1c1f24 };
    const juce::Colour panel2   { 0xff23272d };
    const juce::Colour line     { 0xff2f343c };
    const juce::Colour text     { 0xffdde1e6 };
    const juce::Colour dim      { 0xff858d97 };
    const juce::Colour accent   { 0xfff2a541 };   // ambra
    const juce::Colour accent2  { 0xff57d0b0 };   // menta (modulazione)
    const juce::Colour warn     { 0xffe5584f };
    const juce::Colour knob     { 0xff2b3037 };
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain);
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::accent);
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::buttonOnColourId, col::accent);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::TooltipWindow::backgroundColourId, col::panel2);
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::text);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.5f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (std::min (13.0f, h * 0.55f), true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path p; const float cx = (float) w - 11.0f, cy = h * 0.5f;
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (col::accent); g.fillPath (p);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
        if (over) c = c.brighter (0.08f);
        if (down) c = c.darker (0.15f);
        g.setColour (c); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? c.brighter (0.2f) : col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
};

// Pannello con titolo
inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (col::dim); g.setFont (font (11.5f, true));
        g.drawText (title, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 16, juce::Justification::centredLeft);
        g.setColour (col::line); g.drawHorizontalLine (r.getY() + 22, (float) r.getX() + 8, (float) r.getRight() - 8);
    }
}
} // namespace ui
