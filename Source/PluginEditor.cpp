#include "PluginProcessor.h"
#include "PluginEditor.h"

JoseModAmpAudioProcessorEditor::JoseModAmpAudioProcessorEditor(JoseModAmpAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(820, 480);

    // Front Panel Knobs
    setupRotary(gainSlider, "gain");
    setupRotary(bassSlider, "bass");
    setupRotary(midSlider, "middle");
    setupRotary(trebleSlider, "treble");
    setupRotary(presenceSlider, "presence");
    setupRotary(masterSlider, "master");

    // Workbench Slider (Linear Bar)
    auto setupModSlider = [this](juce::Slider& s, const juce::String& paramId) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        addAndMakeVisible(s);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getAPVTS(), paramId, s));
    };

    setupModSlider(cathodeSlider, "cathode_cap");
    setupModSlider(brightSlider, "bright_cap");
    setupModSlider(slopeSlider, "slope_res");

    // Diode Selector
    diodeSelector.addItem("0: Stock (Pure Tube)", 1);
    diodeSelector.addItem("1: Silicon 1N4148", 2);
    diodeSelector.addItem("2: Jose Mod (Zener 4.7V)", 3);
    diodeSelector.addItem("3: Red LEDs", 4);
    addAndMakeVisible(diodeSelector);
    diodeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "diode_mode", diodeSelector);

    // IR Loader Controls
    addAndMakeVisible(loadIrBtn);
    loadIrBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Wähle eine Speaker IR (.wav)...", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");
        
        // In JUCE 8 reicht 'openMode' vollkommen aus (kein canFiles Flag nötig)
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
    irNameLabel.setColour(juce::Label::textColourId, juce::Colours::goldenrod);
    addAndMakeVisible(irNameLabel);
}

void JoseModAmpAudioProcessorEditor::setupRotary(juce::Slider& s, const juce::String& paramId)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    addAndMakeVisible(s);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramId, s));
}

void JoseModAmpAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Vintage Marshall Head Tolex
    g.fillAll(juce::Colour(0xff121212));

    // Gold Anodized Faceplate (Amp Top)
    auto goldRect = juce::Rectangle<int>(15, 15, getWidth() - 30, 200);
    juce::ColourGradient goldGrad(juce::Colour(0xffd4af37), 0, 15, juce::Colour(0xff7d6012), 0, 215, false);
    g.setGradientFill(goldGrad);
    g.fillRoundedRectangle(goldRect.toFloat(), 6.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(goldRect.toFloat(), 6.0f, 2.0f);

    // Frontplate Titles
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("JCM 800 - 2203 [JOSE ARREDONDO MOD]", 35, 22, 450, 25, juce::Justification::left);

    // Knob Labels
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    const char* labels[] = { "PREAMP", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "MASTER" };
    for (int i = 0; i < 6; ++i)
    {
        g.drawText(labels[i], 35 + i * 125, 175, 90, 20, juce::Justification::centred);
    }

    // Workbench / Modder Bay Background (Amp Bottom)
    auto modRect = juce::Rectangle<int>(15, 230, getWidth() - 30, 235);
    g.setColour(juce::Colour(0xff1a1a1c));
    g.fillRoundedRectangle(modRect.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff333338));
    g.drawRoundedRectangle(modRect.toFloat(), 6.0f, 1.5f);

    g.setColour(juce::Colour(0xffd4af37));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("CIRCUIT MODDING BENCH (INTERAKTIVE BAUTEILE)", 35, 240, 400, 25, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.setColour(juce::Colours::lightgrey);
    g.drawText("V1b Cathode Cap (Tightness / Bass-Kompression):", 35, 275, 300, 20, juce::Justification::left);
    g.drawText("Bright Cap (Top-End Grit / Bite):", 35, 305, 300, 20, juce::Justification::left);
    g.drawText("Slope Resistor (Tonestack Mid Frequency Shift):", 35, 335, 300, 20, juce::Justification::left);
    g.drawText("Clipping Diodes Stufe:", 35, 370, 200, 20, juce::Justification::left);
    g.drawText("Cabinet Impulse Response (.WAV):", 35, 410, 220, 20, juce::Justification::left);
}

void JoseModAmpAudioProcessorEditor::resized()
{
    // Frontplate Knobs
    int startX = 35;
    int spacing = 125;
    gainSlider.setBounds(startX + 0 * spacing, 60, 90, 110);
    bassSlider.setBounds(startX + 1 * spacing, 60, 90, 110);
    midSlider.setBounds(startX + 2 * spacing, 60, 90, 110);
    trebleSlider.setBounds(startX + 3 * spacing, 60, 90, 110);
    presenceSlider.setBounds(startX + 4 * spacing, 60, 90, 110);
    masterSlider.setBounds(startX + 5 * spacing, 60, 90, 110);

    // Workbench Slider
    cathodeSlider.setBounds(340, 275, 430, 22);
    brightSlider.setBounds(340, 305, 430, 22);
    slopeSlider.setBounds(340, 335, 430, 22);
    diodeSelector.setBounds(340, 368, 220, 25);

    // IR Loader Row
    loadIrBtn.setBounds(340, 410, 220, 28);
    irNameLabel.setBounds(570, 410, 150, 28);
    irBypassButton.setBounds(720, 410, 75, 28);
}

bool JoseModAmpAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".wav");
}

void JoseModAmpAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() == 1)
    {
        juce::File wavFile(files[0]);
        audioProcessor.loadCabFile(wavFile);
        irNameLabel.setText(wavFile.getFileName(), juce::dontSendNotification);
    }
}
