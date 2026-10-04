#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class DiezelEinsteinAudioProcessor;

class DiezelEinsteinAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::FileDragAndDropTarget
{
public:
    explicit DiezelEinsteinAudioProcessorEditor(DiezelEinsteinAudioProcessor&);
    ~DiezelEinsteinAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    DiezelEinsteinAudioProcessor& audioProcessor;

    // Diezel Front Panel Knobs
    juce::Slider gainSlider, tightSlider, bassSlider, midSlider, trebleSlider, presenceSlider, deepSlider, masterSlider;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;

    // Mode Selector & Cabinet
    juce::ComboBox modeSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    juce::ToggleButton irBypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> irBypassAttachment;

    juce::TextButton loadIrBtn { "Load Diezel 4x12 IR (.wav)" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label irNameLabel;

    void setupRotary(juce::Slider& slider, const juce::String& paramId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiezelEinsteinAudioProcessorEditor)
};
