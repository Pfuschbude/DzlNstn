#pragma once
#include <juce_dsp/juce_dsp.h>

class DiezelDspEngine
{
public:
    DiezelDspEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // 1. Pre-Tightening Filters (Steilflankig gegen Matsch)
        preHighpass1.prepare(spec);
        preHighpass1.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        preHighpass2.prepare(spec);
        preHighpass2.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        // Pre-Bite Boost (Pick-Attack Akzentuierung)
        for (int ch = 0; ch < 2; ++ch)
            preBiteFilter[ch].prepare(spec);

        // Inter-stage DC Blocker (simuliert Koppelkondensatoren)
        for (int stage = 0; stage < 3; ++stage)
        {
            dcBlocker[stage].prepare(spec);
            dcBlocker[stage].setType(juce::dsp::FirstOrderTPTFilterType::highpass);
            dcBlocker[stage].setCutoffFrequency(80.0f);
        }

        // Post-EQ & Power Amp Resonance (Deep)
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].prepare(spec);
            midFilter[ch].prepare(spec);
            trebleFilter[ch].prepare(spec);
            presenceFilter[ch].prepare(spec);
            deepFilter[ch].prepare(spec);
        }

        cabLoader.prepare(spec);

        updatePreFilters();
        updateToneStack();
    }

    void reset()
    {
        preHighpass1.reset();
        preHighpass2.reset();
        for (int stage = 0; stage < 3; ++stage)
            dcBlocker[stage].reset();

        for (int ch = 0; ch < 2; ++ch)
        {
            preBiteFilter[ch].reset();
            bassFilter[ch].reset();
            midFilter[ch].reset();
            trebleFilter[ch].reset();
            presenceFilter[ch].reset();
            deepFilter[ch].reset();
        }
        cabLoader.reset();
    }

    void setGain(float val)             { gainParam = val; }
    void setMaster(float val)           { masterParam = val; }
    void setTight(float val)            { tightParam = val; updatePreFilters(); }
    void setBass(float val)             { bassParam = val; updateToneStack(); }
    void setMiddle(float val)           { midParam = val; updateToneStack(); }
    void setTreble(float val)           { trebleParam = val; updateToneStack(); }
    void setPresence(float val)         { presenceParam = val; updateToneStack(); }
    void setDeep(float val)             { deepParam = val; updateToneStack(); }
    void setMode(int modeIdx)           { ampMode = modeIdx; updatePreFilters(); }
    void setIrBypass(bool bp)           { irBypass = bp; }

    void loadCabinetIR(const juce::File& file)
    {
        cabLoader.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0);
        hasCustomIr = true;
    }

    template <typename ProcessContext>
    void process(ProcessContext& context)
    {
        auto& block = context.getOutputBlock();
        const size_t numChannels = block.getNumChannels();
        const size_t numSamples = block.getNumSamples();

        float driveMultiplier = 1.0f;
        if (ampMode == 0)      driveMultiplier = 2.0f;   // Clean
        else if (ampMode == 1) driveMultiplier = 7.5f;   // Crunch
        else                   driveMultiplier = 24.0f;  // Mega (Lead)

        const float drive = 1.0f + std::pow(gainParam * driveMultiplier, 1.85f);
        const float outVol = std::pow(masterParam, 1.6f) * 0.75f;

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float x = data[i];
                int fCh = (ch < 2) ? static_cast<int>(ch) : 0;

                // 1. Ultra-Tight Pre-Filtering (Bass cut VOR dem Gain)
                x = preHighpass1.processSample(static_cast<int>(ch), x);
                x = preHighpass2.processSample(static_cast<int>(ch), x);
                x = preBiteFilter[fCh].processSample(x);

                // 2. Kaskadierte 12AX7-Röhrenstufen mit DC-Entkoppelung
                // Stufe 1: Dynamischer Eingang
                x = triodeStage(x * drive * 0.35f, 1.25f);
                x = dcBlocker[0].processSample(static_cast<int>(ch), x);

                // Stufe 2: Crunch-Kern
                x = triodeStage(x * 1.6f, 1.5f);
                x = dcBlocker[1].processSample(static_cast<int>(ch), x);

                // Stufe 3: Mega High-Gain Kompression
                if (ampMode >= 1)
                {
                    float stage3Drive = (ampMode == 2) ? 2.1f : 1.2f;
                    x = triodeStage(x * stage3Drive, 1.8f);
                    x = dcBlocker[2].processSample(static_cast<int>(ch), x);
                }

                // 3. Tonestack & Endstufen-Deep (Hier kommt das Bassfundament zurück)
                x = bassFilter[fCh].processSample(x);
                x = midFilter[fCh].processSample(x);
                x = trebleFilter[fCh].processSample(x);
                x = presenceFilter[fCh].processSample(x);
                x = deepFilter[fCh].processSample(x);

                // 4. Master Volume
                data[i] = x * outVol;
            }
        }

        if (!irBypass && hasCustomIr)
            cabLoader.process(context);
    }

private:
    void updatePreFilters()
    {
        if (sampleRate <= 0.0) return;

        // Der Tight-Regler verschiebt den Bass-Cutoff vor dem Gain
        // Bei voll aufgedrehtem Tight werden Frequenzen bis 260 Hz vor der Zerre gedämpft
        float baseFreq = 70.0f;
        if (ampMode == 1) baseFreq = 120.0f;
        if (ampMode == 2) baseFreq = 160.0f;

        float targetCutoff = juce::jmap(tightParam, 0.0f, 1.0f, baseFreq, baseFreq + 130.0f);
        preHighpass1.setCutoffFrequency(targetCutoff);
        preHighpass2.setCutoffFrequency(targetCutoff * 0.75f);

        // Attack-Peak bei 2.2 kHz für perkussives Plektrum-Ansprechverhalten
        auto biteCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 2200.0f, 1.0f, juce::Decibels::decibelsToGain(juce::jmap(tightParam, 0.0f, 1.0f, 1.0f, 5.0f)));

        for (int ch = 0; ch < 2; ++ch)
            preBiteFilter[ch].coefficients = biteCoeffs;
    }

    void updateToneStack()
    {
        if (sampleRate <= 0.0) return;

        // Bass Low-Shelf (90 Hz)
        auto bassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 90.0f, 0.707f, juce::Decibels::decibelsToGain((bassParam - 0.5f) * 22.0f));

        // Markante Diezel Mittenfrequenz (680 Hz)
        auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 680.0f, 1.2f, juce::Decibels::decibelsToGain((midParam - 0.5f) * 22.0f));

        // Treble High-Shelf (3.6 kHz)
        auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 3600.0f, 0.707f, juce::Decibels::decibelsToGain((trebleParam - 0.5f) * 20.0f));

        // Presence Peaking (5.2 kHz)
        auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 5200.0f, 0.8f, juce::Decibels::decibelsToGain((presenceParam - 0.5f) * 16.0f));

        // DIEZEL DEEP REGELUNG: Resonanzpeak bei 58 Hz (Mächtiger Subbass NACH der Verzerrung)
        auto deepCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 58.0f, 1.8f, juce::Decibels::decibelsToGain(deepParam * 19.0f));

        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].coefficients = bassCoeffs;
            midFilter[ch].coefficients = midCoeffs;
            trebleFilter[ch].coefficients = trebleCoeffs;
            presenceFilter[ch].coefficients = presenceCoeffs;
            deepFilter[ch].coefficients = deepCoeffs;
        }
    }

    // Asymmetrische Triodensättigung mit straffer Kompression
    inline float triodeStage(float in, float asymmetry)
    {
        if (in > 0.0f)
            return std::tanh(in);
        else
            return std::tanh(in * asymmetry) / asymmetry;
    }

    double sampleRate = 48000.0;

    float gainParam     = 0.65f;
    float masterParam   = 0.5f;
    float tightParam    = 0.6f; // Standard: schön straff
    float bassParam     = 0.5f;
    float midParam      = 0.55f;
    float trebleParam   = 0.5f;
    float presenceParam = 0.5f;
    float deepParam     = 0.65f; // Kräftiger Diezel-Punch
    int   ampMode       = 2;     // Mega
    bool  irBypass      = false;
    bool  hasCustomIr   = false;

    juce::dsp::FirstOrderTPTFilter<float> preHighpass1;
    juce::dsp::FirstOrderTPTFilter<float> preHighpass2;
    juce::dsp::IIR::Filter<float> preBiteFilter[2];
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker[3];

    juce::dsp::IIR::Filter<float> bassFilter[2];
    juce::dsp::IIR::Filter<float> midFilter[2];
    juce::dsp::IIR::Filter<float> trebleFilter[2];
    juce::dsp::IIR::Filter<float> presenceFilter[2];
    juce::dsp::IIR::Filter<float> deepFilter[2];

    juce::dsp::Convolution cabLoader;
};
