#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "QpasaLookAndFeel.h"

class QpasaReverbEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit QpasaReverbEditor (QpasaReverbProcessor&);
    ~QpasaReverbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        juce::String hint;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const char* paramId, const juce::String& name, const juce::String& hint);
    void selectMode (int index);
    void refreshPresetBox();
    void stepPreset (int delta);

    QpasaReverbProcessor& processor;
    QpasaLookAndFeel lnf;

    std::array<Knob, 5> knobs;
    std::array<juce::TextButton, 4> modeButtons;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    int shownMode = -1;
    int shownPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QpasaReverbEditor)
};
