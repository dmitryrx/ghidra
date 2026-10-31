#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;

QpasaReverbProcessor::QpasaReverbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QpasaReverbState", createLayout())
{
    pMix      = apvts.getRawParameterValue (ParamIDs::mix);
    pDecay    = apvts.getRawParameterValue (ParamIDs::decay);
    pPreDelay = apvts.getRawParameterValue (ParamIDs::preDelay);
    pTone     = apvts.getRawParameterValue (ParamIDs::tone);
    pMod      = apvts.getRawParameterValue (ParamIDs::mod);
    pMode     = apvts.getRawParameterValue (ParamIDs::mode);
}

APVTS::ParameterLayout QpasaReverbProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto percent = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })
                       .withLabel ("%");

    NormalisableRange<float> decayRange (0.2f, 20.0f, 0.01f);
    decayRange.setSkewForCentre (2.5f);
    NormalisableRange<float> preRange (0.0f, 250.0f, 0.1f);
    preRange.setSkewForCentre (50.0f);

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                             NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::decay, 1 }, "Decay", decayRange, 2.5f,
        AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? String (v, 2) + " s" : String (v, 1) + " s"; })));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::preDelay, 1 }, "Pre-Delay", preRange, 20.0f,
        AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tone, 1 }, "Tone",
                                                             NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mod, 1 }, "Mod",
                                                             NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, percent));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode",
                                                              StringArray { "Room", "Hall", "Plate", "Ambient" }, 1));

    return { params.begin(), params.end() };
}

bool QpasaReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return in.size() <= out.size();
}

void QpasaReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    monoScratch.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);

    qpasa::Params p;
    p.mix = pMix->load() / 100.0f;
    p.decaySec = pDecay->load();
    p.preDelayMs = pPreDelay->load();
    p.tone = pTone->load() / 100.0f;
    p.mod = pMod->load() / 100.0f;
    p.mode = (qpasa::Mode) juce::jlimit (0, qpasa::kNumModes - 1, (int) pMode->load());
    engine.setParams (p);
    engine.prepare (sampleRate);
}

void QpasaReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numIn == 0 || numOut == 0 || numSamples == 0)
        return;

    qpasa::Params p;
    p.mix = pMix->load() / 100.0f;
    p.decaySec = pDecay->load();
    p.preDelayMs = pPreDelay->load();
    p.tone = pTone->load() / 100.0f;
    p.mod = pMod->load() / 100.0f;
    p.mode = (qpasa::Mode) juce::jlimit (0, qpasa::kNumModes - 1, (int) pMode->load());
    engine.setParams (p);

    float* left = buffer.getWritePointer (0);

    if (numOut >= 2)
    {
        float* right = buffer.getWritePointer (1);
        if (numIn == 1)
            juce::FloatVectorOperations::copy (right, left, numSamples); // mono in -> stereo out
        engine.process (left, right, numSamples);
    }
    else
    {
        // mono out: run stereo engine and fold down
        if (monoScratch.size() < (size_t) numSamples)
            monoScratch.resize ((size_t) numSamples); // only if the host breaks its block-size promise
        float* tmp = monoScratch.data();
        juce::FloatVectorOperations::copy (tmp, left, numSamples);
        engine.process (left, tmp, numSamples);
        juce::FloatVectorOperations::add (left, tmp, numSamples);
        juce::FloatVectorOperations::multiply (left, 0.5f, numSamples);
    }

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
int QpasaReverbProcessor::getNumPrograms() { return qpasa::kNumPresets; }

const juce::String QpasaReverbProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, qpasa::kNumPresets))
        return qpasa::kPresets[index].name;
    return {};
}

void QpasaReverbProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, qpasa::kNumPresets))
        return;

    currentPreset = index;
    const auto& pr = qpasa::kPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (ParamIDs::mode, (float) (int) pr.mode);
    set (ParamIDs::mix, pr.mix);
    set (ParamIDs::decay, pr.decay);
    set (ParamIDs::preDelay, pr.preDelay);
    set (ParamIDs::tone, pr.tone);
    set (ParamIDs::mod, pr.mod);
}

//==============================================================================
void QpasaReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void QpasaReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, qpasa::kNumPresets - 1, (int) tree.getProperty ("preset", 0));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* QpasaReverbProcessor::createEditor()
{
    return new QpasaReverbEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QpasaReverbProcessor();
}
