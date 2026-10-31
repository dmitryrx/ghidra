#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Flat, minimal look for Qpasa Reverb. Each mode has its own accent colour.
namespace QpasaColours
{
    inline const juce::Colour background  { 0xff15171c };
    inline const juce::Colour panel       { 0xff1e2128 };
    inline const juce::Colour panelLight  { 0xff2a2e37 };
    inline const juce::Colour track       { 0xff353a45 };
    inline const juce::Colour text        { 0xffeceef2 };
    inline const juce::Colour textMuted   { 0xff8a909c };

    inline juce::Colour accentForMode (int mode)
    {
        switch (mode)
        {
            case 0:  return juce::Colour (0xffffb547); // Room    - amber
            case 1:  return juce::Colour (0xff3fd2c7); // Hall    - teal
            case 2:  return juce::Colour (0xffff6b6b); // Plate   - coral
            default: return juce::Colour (0xffa98bff); // Ambient - lavender
        }
    }
}

class QpasaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QpasaLookAndFeel()
    {
        using namespace QpasaColours;
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, textMuted);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, panelLight);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, panelLight);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::textColourOffId, textMuted);
        setColour (juce::TextButton::textColourOnId, background);
    }

    void setAccent (juce::Colour c) { accent = c; }
    juce::Colour getAccent() const { return accent; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace QpasaColours;
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float ringWidth = radius * 0.11f;
        const float arcRadius = radius - ringWidth * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // background ring
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (track);
        g.strokePath (bgArc, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value ring
        if (pos > 0.001f)
        {
            juce::Path valArc;
            valArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
            g.setColour (slider.isEnabled() ? accent : textMuted);
            g.strokePath (valArc, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body
        const float bodyR = radius * 0.70f;
        const bool hot = slider.isMouseOverOrDragging();
        g.setColour (hot ? panelLight : panel);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        // pointer dot
        const float dotR = juce::jmax (2.5f, radius * 0.07f);
        const float dotDist = bodyR * 0.68f;
        const juce::Point<float> dot (centre.x + dotDist * std::sin (angle), centre.y - dotDist * std::cos (angle));
        g.setColour (text);
        g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool isOver, bool isDown) override
    {
        using namespace QpasaColours;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const float corner = r.getHeight() * 0.5f;
        if (b.getToggleState())
            g.setColour (accent);
        else
            g.setColour (isDown ? panelLight.brighter (0.1f) : (isOver ? panelLight : panel));
        g.fillRoundedRectangle (r, corner);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace QpasaColours;
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.setColour (b.getToggleState() ? background : (b.isMouseOver() ? text : textMuted));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace QpasaColours;
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.isMouseOver (true) ? panelLight : panel);
        g.fillRoundedRectangle (r, 8.0f);

        juce::Path arrow;
        const float ax = (float) width - 18.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (textMuted);
        g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 0, box.getWidth() - 34, box.getHeight());
        label.setFont (juce::FontOptions (15.0f));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (15.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineWhenEditingColourId, QpasaColours::panelLight);
        l->setColour (juce::Label::backgroundWhenEditingColourId, QpasaColours::panel);
        return l;
    }

private:
    juce::Colour accent = QpasaColours::accentForMode (1);
};
