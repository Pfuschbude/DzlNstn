#include "PluginProcessor.h"
#include "PluginEditor.h"

JoseModAmpAudioProcessor::JoseModAmpAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout JoseModAmpAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Front Panel Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tight", "Tightness", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bass", "Bass", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("middle", "Middle", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("treble", "Treble", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("presence", "Presence", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("deep", "Deep", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("master", "Master", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));

    // Mode Selector & Cabinet
    params.push_back(std::make_unique<juce::AudioParameterChoice>("amp_mode", "Amp Mode", juce::StringArray{"Clean", "Crunch", "Mega"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>("ir_bypass", "Bypass IR Cab", false));

    return { params.begin(), params.end() };
}

void JoseModAmpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dspEngine.prepare(spec);
}

void JoseModAmpAudioProcessor::releaseResources()
{
    dspEngine.reset();
}

bool JoseModAmpAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void JoseModAmpAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    dspEngine.setGain(*apvts.getRawParameterValue("gain"));
    dspEngine.setTight(*apvts.getRawParameterValue("tight"));
    dspEngine.setBass(*apvts.getRawParameterValue("bass"));
    dspEngine.setMiddle(*apvts.getRawParameterValue("middle"));
    dspEngine.setTreble(*apvts.getRawParameterValue("treble"));
    dspEngine.setPresence(*apvts.getRawParameterValue("presence"));
    dspEngine.setDeep(*apvts.getRawParameterValue("deep"));
    dspEngine.setMaster(*apvts.getRawParameterValue("master"));

    dspEngine.setMode(static_cast<int>(*apvts.getRawParameterValue("amp_mode")));
    dspEngine.setIrBypass(*apvts.getRawParameterValue("ir_bypass") > 0.5f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dspEngine.process(context);
}

juce::AudioProcessorEditor* JoseModAmpAudioProcessor::createEditor()
{
    return new JoseModAmpAudioProcessorEditor(*this);
}

void JoseModAmpAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JoseModAmpAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JoseModAmpAudioProcessor();
}
