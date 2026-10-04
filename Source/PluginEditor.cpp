#include "PluginProcessor.h"
#include "PluginEditor.h"

DiezelEinsteinAudioProcessorEditor::DiezelEinsteinAudioProcessorEditor(DiezelEinsteinAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1000, 420);

    // 8 Kanal-Knobs initialisieren
    setupRotary(gainSlider);
    setupRotary(tightSlider);
    setupRotary(bassSlider);
    setupRotary(midSlider);
    setupRotary(trebleSlider);
    setupRotary(presenceSlider);
    setupRotary(deepSlider);
    setupRotary(masterSlider);

    // Globaler Noise Gate Knob
    setupRotary(gateSlider);
    gateSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00e5ff));
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "gate", gateSlider);

    // TS Screamer Boost Button
    tsBoostButton.setButtonText("TS SCREAMER BOOST");
    tsBoostButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff00e5ff));
    tsBoostButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(tsBoostButton);

    // Mode Selector (Clean / Crunch / Mega)
    modeSelector.addItem("Mode 1: CLEAN", 1);
    modeSelector.addItem("Mode 2: CRUNCH", 2);
    modeSelector.addItem("Mode 3: MEGA", 3);
    addAndMakeVisible(modeSelector);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "amp_mode", modeSelector);

    // Bei Umschalten des Modus: Regler sofort auf die gespeicherten Werte dieses Modus umschalten
    modeSelector.onChange = [this]() {
        updateChannelAttachments();
    };

    // Initiale Kanal-Anbindung herstellen
    updateChannelAttachments();

    // Dual IR Loader A
    addAndMakeVisible(loadIrBtnA);
    loadIrBtnA.onClick = [this]() {
        fileChooserA = std::make_unique<juce::FileChooser>(
            "Wähle Speaker IR A (.wav)...", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        fileChooserA->launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    audioProcessor.loadCabFileA(file);
                    irNameLabelA.setText("A: " + file.getFileName(), juce::dontSendNotification);
                }
            });
    };
    irNameLabelA.setText("Cab A: Standard", juce::dontSendNotification);
    irNameLabelA.setColour(juce::Label::textColourId, juce::Colour(0xff4fc3f7));
    addAndMakeVisible(irNameLabelA);

    // Dual IR Loader B
    addAndMakeVisible(loadIrBtnB);
    loadIrBtnB.onClick = [this]() {
        fileChooserB = std::make_unique<juce::FileChooser>(
            "Wähle Speaker IR B (.wav)...", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        fileChooserB->launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    audioProcessor.loadCabFileB(file);
                    irNameLabelB.setText("B: " + file.getFileName(), juce::dontSendNotification);
                }
            });
    };
    irNameLabelB.setText("Cab B: Leer", juce::dontSendNotification);
    irNameLabelB.setColour(juce::Label::textColourId, juce::Colour(0xff4fc3f7));
    addAndMakeVisible(irNameLabelB);

    // IR Blend Fader (A <---> B)
    irBlendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    irBlendSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    irBlendSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(irBlendSlider);
    irBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "ir_blend", irBlendSlider);

    // Bypass Cab Button
    irBypassButton.setButtonText("Bypass Cab");
    addAndMakeVisible(irBypassButton);
    irBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "ir_bypass", irBypassButton);
}

void DiezelEinsteinAudioProcessorEditor::setupRotary(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 18);
    addAndMakeVisible(s);
}

void DiezelEinsteinAudioProcessorEditor::attachRotary(juce::Slider& s, const juce::String& paramId)
{
    channelSliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramId, s));
}

void DiezelEinsteinAudioProcessorEditor::updateChannelAttachments()
{
    channelSliderAttachments.clear();
    tsBoostAttachment.reset();

    int mode = modeSelector.getSelectedId() - 1;
    if (mode < 0 || mode > 2) mode = 1;

    juce::String prefix = (mode == 0) ? "c_" : (mode == 1 ? "cr_" : "mg_");

    attachRotary(gainSlider, prefix + "gain");
    attachRotary(tightSlider, prefix + "tight");
    attachRotary(bassSlider, prefix + "bass");
    attachRotary(midSlider, prefix + "middle");
    attachRotary(trebleSlider, prefix + "treble");
    attachRotary(presenceSlider, prefix + "presence");
    attachRotary(deepSlider, prefix + "deep");
    attachRotary(masterSlider, prefix + "master");

    tsBoostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), prefix + "ts_boost", tsBoostButton);
}

void DiezelEinsteinAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Diezel Anthrazit Chassis
    g.fillAll(juce::Colour(0xff141416));

    // Gebürstetes Aluminium / Dunkle Frontplatte
    auto plate = juce::Rectangle<int>(15, 12, getWidth() - 30, 245);
    juce::ColourGradient plateGrad(juce::Colour(0xff2d2e33), 0, 15, juce::Colour(0xff1b1c1e), 0, 255, false);
    g.setGradientFill(plateGrad);
    g.fillRoundedRectangle(plate.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff44464f));
    g.drawRoundedRectangle(plate.toFloat(), 4.0f, 1.5f);

    // Diezel Typenschild
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("DIEZEL", 35, 20, 120, 25, juce::Justification::left);

    g.setColour(juce::Colour(0xff00b0ff));
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("EINSTEIN 100", 135, 22, 160, 25, juce::Justification::left);

    g.setColour(juce::Colour(0xff888a95));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("MULTI-CHANNEL MEMORY & TS BOOST", 270, 24, 250, 25, juce::Justification::left);

    // Regler-Labels (9 Regler)
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    const char* labels[] = { "GAIN", "TIGHT", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "DEEP", "MASTER", "GATE" };
    int startX = 35;
    int spacing = 104;

    for (int i = 0; i < 9; ++i)
    {
        if (i == 1 || i == 6)
            g.setColour(juce::Colour(0xff00b0ff));
        else if (i == 8)
            g.setColour(juce::Colour(0xff00e5ff)); // Gate in Cyan
        else
            g.setColour(juce::Colour(0xffcfd8dc));

        g.drawText(labels[i], startX + i * spacing, 175, 80, 20, juce::Justification::centred);
    }

    // Untere Leiste: DUAL CABINET IR STUDIO & BLEND
    auto botBar = juce::Rectangle<int>(15, 270, getWidth() - 30, 135);
    g.setColour(juce::Colour(0xff1c1d21));
    g.fillRoundedRectangle(botBar.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff33353b));
    g.drawRoundedRectangle(botBar.toFloat(), 4.0f, 1.0f);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff00b0ff));
    g.drawText("DUAL CABINET IR STUDIO & BLEND", 35, 280, 280, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xffcfd8dc));
    g.drawText("CAB A <--- BLEND ---> CAB B", 390, 310, 220, 18, juce::Justification::centred);
}

void DiezelEinsteinAudioProcessorEditor::resized()
{
    int startX = 35;
    int spacing = 104;

    // 9 Front-Knobs
    gainSlider.setBounds(startX + 0 * spacing, 55, 80, 115);
    tightSlider.setBounds(startX + 1 * spacing, 55, 80, 115);
    bassSlider.setBounds(startX + 2 * spacing, 55, 80, 115);
    midSlider.setBounds(startX + 3 * spacing, 55, 80, 115);
    trebleSlider.setBounds(startX + 4 * spacing, 55, 80, 115);
    presenceSlider.setBounds(startX + 5 * spacing, 55, 80, 115);
    deepSlider.setBounds(startX + 6 * spacing, 55, 80, 115);
    masterSlider.setBounds(startX + 7 * spacing, 55, 80, 115);
    gateSlider.setBounds(startX + 8 * spacing, 55, 80, 115);

    // Channel Selector & TS Boost Button
    modeSelector.setBounds(35, 205, 170, 28);
    tsBoostButton.setBounds(225, 205, 190, 28);

    // Untere Leiste: Dual IR Loader & Blend Fader
    loadIrBtnA.setBounds(35, 320, 140, 26);
    irNameLabelA.setBounds(35, 350, 180, 22);

    irBlendSlider.setBounds(390, 335, 220, 35);

    loadIrBtnB.setBounds(650, 320, 140, 26);
    irNameLabelB.setBounds(650, 350, 180, 22);

    irBypassButton.setBounds(850, 320, 110, 26);
}

bool DiezelEinsteinAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() >= 1 && files[0].endsWithIgnoreCase(".wav");
}

void DiezelEinsteinAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() >= 1)
    {
        juce::File wavFile(files[0]);
        audioProcessor.loadCabFileA(wavFile);
        irNameLabelA.setText("A: " + wavFile.getFileName(), juce::dontSendNotification);
    }
}
