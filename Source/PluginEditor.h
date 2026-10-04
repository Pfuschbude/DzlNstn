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

    // Front Panel Knobs
    juce::Slider gainSlider, tightSlider, bassSlider, midSlider, trebleSlider, presenceSlider, deepSlider, masterSlider;
    juce::Slider gateSlider;

    // Kanal-spezifische Attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> channelSliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tsBoostAttachment;

    // Globale Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> irBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> irBypassAttachment;

    // Mode Selector & TS Boost
    juce::ComboBox modeSelector;
    juce::ToggleButton tsBoostButton;

    // Dual IR Loader Controls
    juce::TextButton loadIrBtnA { "Load Cab A" };
    juce::TextButton prevIrBtnA { "<" };
    juce::TextButton nextIrBtnA { ">" };
    juce::Label irNameLabelA;

    juce::TextButton loadIrBtnB { "Load Cab B" };
    juce::TextButton prevIrBtnB { "<" };
    juce::TextButton nextIrBtnB { ">" };
    juce::Label irNameLabelB;

    juce::Slider irBlendSlider;
    juce::ToggleButton irBypassButton;

    std::unique_ptr<juce::FileChooser> fileChooserA;
    std::unique_ptr<juce::FileChooser> fileChooserB;

    // Preset Buttons
    juce::TextButton savePresetBtn { "Save Preset" };
    juce::TextButton loadPresetBtn { "Load Preset" };
    std::unique_ptr<juce::FileChooser> presetFileChooser;

    // Ordner-Dateilisten zum Durchschalten
    juce::Array<juce::File> irFilesA;
    int currentIrIndexA = -1;
    juce::Array<juce::File> irFilesB;
    int currentIrIndexB = -1;

    void scanIrFolderA(const juce::File& fileInFolder);
    void scanIrFolderB(const juce::File& fileInFolder);
    void selectIrIndexA(int index);
    void selectIrIndexB(int index);

    void setupRotary(juce::Slider& slider);
    void attachRotary(juce::Slider& slider, const juce::String& paramId);
    void updateChannelAttachments();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiezelEinsteinAudioProcessorEditor)
};
