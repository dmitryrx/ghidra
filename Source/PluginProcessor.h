#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ReverbEngine.h"

namespace ParamIDs
{
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* preDelay = "predelay";
    inline constexpr const char* tone     = "tone";
    inline constexpr const char* mod      = "mod";
    inline constexpr const char* mode     = "mode";
}

class QpasaReverbProcessor : public juce::AudioProcessor
{
public:
    QpasaReverbProcessor();
    ~QpasaReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    qpasa::ReverbEngine engine;
    int currentPreset = 0;
    std::vector<float> monoScratch;

    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pPreDelay = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pMod = nullptr;
    std::atomic<float>* pMode = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QpasaReverbProcessor)
};
