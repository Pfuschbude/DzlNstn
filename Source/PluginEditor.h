#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declaration verhindert die Endlosschleife
class JoseModAmpAudioProcessor;

class JoseModAmpAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget
{
public:
    explicit JoseModAmpAudioProcessorEditor(JoseModAmpAudioProcessor&);
    ~JoseModAmpAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    JoseModAmpAudioProcessor& audioProcessor;

    // Marshall Front Panel Controls
    juce::Slider gainSlider, bassSlider, midSlider, trebleSlider, presenceSlider, masterSlider;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;

    // Modding Workbench Controls (Bauteile)
    juce::Slider cathodeSlider, brightSlider, slopeSlider;
    juce::ComboBox diodeSelector;
    juce::ToggleButton irBypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> diodeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> irBypassAttachment;

    juce::TextButton loadIrBtn { "Load Cab .WAV (oder Drag&Drop)" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label irNameLabel;

    void setupRotary(juce::Slider& slider, const juce::String& paramId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JoseModAmpAudioProcessorEditor)
};
