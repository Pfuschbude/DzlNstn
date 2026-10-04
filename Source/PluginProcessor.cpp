#include "PluginProcessor.h"
#include "PluginEditor.h"

DiezelEinsteinAudioProcessor::DiezelEinsteinAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DiezelEinsteinAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Globale Controls
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "amp_mode", "Amp Mode", juce::StringArray{ "Mode 1: CLEAN", "Mode 2: CRUNCH", "Mode 3: MEGA" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gate", "Noise Gate", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ir_blend", "IR Cab A/B Blend", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "ir_bypass", "Bypass IR Cab", false));

    // Helper Lambda für die 3 individuellen Kanal-Speicher (Clean, Crunch, Mega)
    auto addChannelParams = [&](const juce::String& p, const juce::String& name, float defGain, float defDeep, bool defTs)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "gain", name + " Gain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), defGain));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "tight", name + " Tight", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "bass", name + " Bass", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "middle", name + " Middle", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "treble", name + " Treble", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "presence", name + " Presence", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "deep", name + " Deep", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), defDeep));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            p + "master", name + " Master", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            p + "ts_boost", name + " TS Screamer", defTs));
    };

    addChannelParams("c_", "Clean", 0.35f, 0.45f, false);
    addChannelParams("cr_", "Crunch", 0.70f, 0.65f, true);
    addChannelParams("mg_", "Mega", 0.65f, 0.70f, false);

    return { params.begin(), params.end() };
}

void DiezelEinsteinAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dspEngine.prepare(spec);
}

void DiezelEinsteinAudioProcessor::releaseResources()
{
    dspEngine.reset();
}

bool DiezelEinsteinAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void DiezelEinsteinAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Aktuellen Modus auslesen
    int mode = static_cast<int>(*apvts.getRawParameterValue("amp_mode"));
    juce::String prefix = (mode == 0) ? "c_" : (mode == 1 ? "cr_" : "mg_");

    dspEngine.setMode(mode);
    dspEngine.setGain(*apvts.getRawParameterValue(prefix + "gain"));
    dspEngine.setTight(*apvts.getRawParameterValue(prefix + "tight"));
    dspEngine.setBass(*apvts.getRawParameterValue(prefix + "bass"));
    dspEngine.setMiddle(*apvts.getRawParameterValue(prefix + "middle"));
    dspEngine.setTreble(*apvts.getRawParameterValue(prefix + "treble"));
    dspEngine.setPresence(*apvts.getRawParameterValue(prefix + "presence"));
    dspEngine.setDeep(*apvts.getRawParameterValue(prefix + "deep"));
    dspEngine.setMaster(*apvts.getRawParameterValue(prefix + "master"));
    dspEngine.setTsBoost(*apvts.getRawParameterValue(prefix + "ts_boost") > 0.5f);

    dspEngine.setGate(*apvts.getRawParameterValue("gate"));
    dspEngine.setIrBlend(*apvts.getRawParameterValue("ir_blend"));
    dspEngine.setIrBypass(*apvts.getRawParameterValue("ir_bypass") > 0.5f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dspEngine.process(context);
}

juce::AudioProcessorEditor* DiezelEinsteinAudioProcessor::createEditor()
{
    return new DiezelEinsteinAudioProcessorEditor(*this);
}

void DiezelEinsteinAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DiezelEinsteinAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiezelEinsteinAudioProcessor();
}
