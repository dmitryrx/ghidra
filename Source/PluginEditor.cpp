#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    constexpr int kWidth  = 760;
    constexpr int kHeight = 410;

    juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

QpasaReverbEditor::QpasaReverbEditor (QpasaReverbProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (knobs[0], ParamIDs::mix,      "MIX",       utf8 ("dry \xe2\x86\x94 wet"));
    setupKnob (knobs[1], ParamIDs::decay,    "DECAY",     "length of the tail");
    setupKnob (knobs[2], ParamIDs::preDelay, "PRE-DELAY", "gap before the reverb");
    setupKnob (knobs[3], ParamIDs::tone,     "TONE",      utf8 ("dark \xe2\x86\x94 bright"));
    setupKnob (knobs[4], ParamIDs::mod,      "MOD",       "gentle movement");

    const char* modeNames[] = { "ROOM", "HALL", "PLATE", "AMBIENT" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { selectMode (i); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < qpasa::kNumPresets; ++i)
        presetBox.addItem (qpasa::kPresets[i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != processor.getCurrentProgram())
            processor.setCurrentProgram (idx);
        shownPreset = idx;
    };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    setSize (kWidth, kHeight);
    timerCallback();
    startTimerHz (20);
}

QpasaReverbEditor::~QpasaReverbEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void QpasaReverbEditor::setupKnob (Knob& k, const char* paramId, const juce::String& name, const juce::String& hint)
{
    addAndMakeVisible (k.slider);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 110, 24);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setMouseDragSensitivity (220);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);

    if (auto* param = processor.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, QpasaColours::textMuted);
    addAndMakeVisible (k.label);

    k.hint = hint;
}

void QpasaReverbEditor::selectMode (int index)
{
    if (auto* param = processor.apvts.getParameter (ParamIDs::mode))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
    timerCallback();
}

void QpasaReverbEditor::stepPreset (int delta)
{
    const int n = qpasa::kNumPresets;
    const int idx = ((processor.getCurrentProgram() + delta) % n + n) % n;
    processor.setCurrentProgram (idx);
    refreshPresetBox();
}

void QpasaReverbEditor::refreshPresetBox()
{
    const int idx = processor.getCurrentProgram();
    if (idx != shownPreset)
    {
        shownPreset = idx;
        presetBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    }
}

void QpasaReverbEditor::timerCallback()
{
    const int mode = juce::jlimit (0, 3, (int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load());
    if (mode != shownMode)
    {
        shownMode = mode;
        lnf.setAccent (QpasaColours::accentForMode (mode));
        for (int i = 0; i < 4; ++i)
            modeButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
        repaint();
    }
    refreshPresetBox();
}

void QpasaReverbEditor::paint (juce::Graphics& g)
{
    using namespace QpasaColours;
    g.fillAll (background);

    // title
    auto header = juce::Rectangle<int> (28, 18, 320, 40);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.setColour (text);
    g.drawText ("qpasa", header, juce::Justification::centredLeft);
    const int titleW = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)), "qpasa");
    g.setFont (juce::FontOptions (28.0f));
    g.setColour (lnf.getAccent());
    g.drawText ("reverb", header.withTrimmedLeft (titleW + 8), juce::Justification::centredLeft);

    // knob panel
    auto panelArea = juce::Rectangle<float> (20.0f, 136.0f, (float) kWidth - 40.0f, 236.0f);
    g.setColour (panel.withAlpha (0.55f));
    g.fillRoundedRectangle (panelArea, 18.0f);

    // hints under the knobs
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (textMuted);
    for (auto& k : knobs)
    {
        auto r = k.slider.getBounds();
        g.drawText (k.hint, r.getX() - 10, r.getBottom() + 2, r.getWidth() + 20, 16, juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (textMuted.withAlpha (0.7f));
    g.drawText ("double-click a knob to reset it", 0, kHeight - 30, kWidth, 18, juce::Justification::centred);
}

void QpasaReverbEditor::resized()
{
    // header: preset selector
    const int comboW = 230, arrowW = 32, h = 34, top = 21;
    nextButton.setBounds (kWidth - 28 - arrowW, top, arrowW, h);
    presetBox.setBounds (nextButton.getX() - 6 - comboW, top, comboW, h);
    prevButton.setBounds (presetBox.getX() - 6 - arrowW, top, arrowW, h);

    // mode buttons
    const int bw = 120, bh = 34, gap = 10;
    const int total = 4 * bw + 3 * gap;
    int x = (kWidth - total) / 2;
    for (auto& b : modeButtons)
    {
        b.setBounds (x, 82, bw, bh);
        x += bw + gap;
    }

    // knobs
    const int colW = (kWidth - 40) / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto& k = knobs[(size_t) i];
        const int cx = 20 + colW * i;
        k.label.setBounds (cx, 148, colW, 18);
        k.slider.setBounds (cx + (colW - 120) / 2, 170, 120, 150);
    }
}
