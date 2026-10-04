#include "PluginProcessor.h"
#include "PluginEditor.h"

DiezelEinsteinAudioProcessorEditor::DiezelEinsteinAudioProcessorEditor(DiezelEinsteinAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(920, 380);

    // Knobs registrieren
    setupRotary(gainSlider, "gain");
    setupRotary(tightSlider, "tight");
    setupRotary(bassSlider, "bass");
    setupRotary(midSlider, "middle");
    setupRotary(trebleSlider, "treble");
    setupRotary(presenceSlider, "presence");
    setupRotary(deepSlider, "deep");
    setupRotary(masterSlider, "master");

    // Mode Selector (Clean / Crunch / Mega)
    modeSelector.addItem("Mode 1: CLEAN", 1);
    modeSelector.addItem("Mode 2: CRUNCH", 2);
    modeSelector.addItem("Mode 3: MEGA", 3);
    addAndMakeVisible(modeSelector);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "amp_mode", modeSelector);

    // IR Loader Controls
    addAndMakeVisible(loadIrBtn);
    loadIrBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Wähle Speaker IR (.wav)...", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    audioProcessor.loadCabFile(file);
                    irNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                }
            });
    };

    irBypassButton.setButtonText("Bypass Cab");
    addAndMakeVisible(irBypassButton);
    irBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "ir_bypass", irBypassButton);

    irNameLabel.setText("Keine IR geladen", juce::dontSendNotification);
    irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff4fc3f7)); // Helles Diezel-Blau
    addAndMakeVisible(irNameLabel);
}

void DiezelEinsteinAudioProcessorEditor::setupRotary(juce::Slider& s, const juce::String& paramId)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 18);
    addAndMakeVisible(s);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramId, s));
}

void DiezelEinsteinAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Diezel Anthrazit Chassis
    g.fillAll(juce::Colour(0xff161618));

    // Gebürstetes Aluminium / Dunkle Frontplatte
    auto plate = juce::Rectangle<int>(15, 15, getWidth() - 30, 230);
    juce::ColourGradient plateGrad(juce::Colour(0xff2d2e33), 0, 15, juce::Colour(0xff1b1c1e), 0, 245, false);
    g.setGradientFill(plateGrad);
    g.fillRoundedRectangle(plate.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff44464f));
    g.drawRoundedRectangle(plate.toFloat(), 4.0f, 1.5f);

    // Diezel Typenschild
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("DIEZEL", 35, 25, 140, 25, juce::Justification::left);

    g.setColour(juce::Colour(0xff00b0ff)); // Diezel Cyan-Blau
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("EINSTEIN 100", 145, 27, 200, 25, juce::Justification::left);

    // Regler-Labels
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xffcfd8dc));

    const char* labels[] = { "GAIN", "TIGHT", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "DEEP", "MASTER" };
    int startX = 35;
    int spacing = 105;

    for (int i = 0; i < 8; ++i)
    {
        // "TIGHT" und "DEEP" farblich hervorheben
        if (i == 1 || i == 6)
            g.setColour(juce::Colour(0xff00b0ff));
        else
            g.setColour(juce::Colour(0xffcfd8dc));

        g.drawText(labels[i], startX + i * spacing, 195, 80, 20, juce::Justification::centred);
    }

    // Untere Leiste (Cabinet / Modi)
    auto botBar = juce::Rectangle<int>(15, 260, getWidth() - 30, 100);
    g.setColour(juce::Colour(0xff1f2024));
    g.fillRoundedRectangle(botBar.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff33353b));
    g.drawRoundedRectangle(botBar.toFloat(), 4.0f, 1.0f);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText("CHANNEL 1 MODE:", 35, 280, 150, 25, juce::Justification::left);
    g.drawText("CABINET IR LOADER:", 35, 320, 150, 25, juce::Justification::left);
}

void DiezelEinsteinAudioProcessorEditor::resized()
{
    int startX = 35;
    int spacing = 105;

    // 8 Front-Knobs
    gainSlider.setBounds(startX + 0 * spacing, 70, 80, 115);
    tightSlider.setBounds(startX + 1 * spacing, 70, 80, 115);
    bassSlider.setBounds(startX + 2 * spacing, 70, 80, 115);
    midSlider.setBounds(startX + 3 * spacing, 70, 80, 115);
    trebleSlider.setBounds(startX + 4 * spacing, 70, 80, 115);
    presenceSlider.setBounds(startX + 5 * spacing, 70, 80, 115);
    deepSlider.setBounds(startX + 6 * spacing, 70, 80, 115);
    masterSlider.setBounds(startX + 7 * spacing, 70, 80, 115);

    // Untere Leiste
    modeSelector.setBounds(180, 278, 200, 26);
    loadIrBtn.setBounds(180, 318, 240, 26);
    irNameLabel.setBounds(435, 318, 300, 26);
    irBypassButton.setBounds(760, 318, 120, 26);
}

bool DiezelEinsteinAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".wav");
}

void DiezelEinsteinAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() == 1)
    {
        juce::File wavFile(files[0]);
        audioProcessor.loadCabFile(wavFile);
        irNameLabel.setText(wavFile.getFileName(), juce::dontSendNotification);
    }
}
