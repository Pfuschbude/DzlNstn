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

    // Amp Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bass", "Bass", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("middle", "Middle", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("treble", "Treble", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("presence", "Presence", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("master", "Master", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));

    // Modding Workbench Parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>("cathode_cap", "V1b Cathode Cap (uF)", juce::NormalisableRange<float>(0.47f, 22.0f, 0.01f), 0.68f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bright_cap", "Bright Cap (pF)", juce::NormalisableRange<float>(0.0f, 4700.0f, 10.0f), 1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("slope_res", "Slope Resistor (Ohm)", juce::NormalisableRange<float>(22000.0f, 56000.0f, 500.0f), 33000.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("diode_mode", "Diode Clipping Mode", juce::StringArray{"Stock", "1N4148", "Jose Zener", "LED"}, 2));
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

    // Parameterwerte abgreifen und an DSP Engine übergeben
    dspEngine.setPreampGain(*apvts.getRawParameterValue("gain"));
    dspEngine.setMasterVolume(*apvts.getRawParameterValue("master"));
    dspEngine.setBass(*apvts.getRawParameterValue("bass"));
    dspEngine.setMiddle(*apvts.getRawParameterValue("middle"));
    dspEngine.setTreble(*apvts.getRawParameterValue("treble"));
    dspEngine.setPresence(*apvts.getRawParameterValue("presence"));

    dspEngine.setCathodeCapValue(*apvts.getRawParameterValue("cathode_cap"));
    dspEngine.setBrightCapValue(*apvts.getRawParameterValue("bright_cap"));
    dspEngine.setSlopeResistor(*apvts.getRawParameterValue("slope_res"));
    dspEngine.setDiodeMode(static_cast<int>(*apvts.getRawParameterValue("diode_mode")));
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
