#pragma once
#include <juce_dsp/juce_dsp.h>

class DiezelDspEngine
{
public:
    DiezelDspEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Puffer für IR B vorbereiten
        tempBufferB.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));

        // TS Screamer Boost Filter (TS808)
        tsHighpass.prepare(spec);
        tsHighpass.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
        tsHighpass.setCutoffFrequency(720.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            tsMidHump[ch].prepare(spec);
            megaMidScoop[ch].prepare(spec);
        }

        auto tsMidCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 720.0f, 0.85f, juce::Decibels::decibelsToGain(8.0f));
        for (int ch = 0; ch < 2; ++ch)
            tsMidHump[ch].coefficients = tsMidCoeffs;

        // Mega Voicing: Gezielter Entmatschungs-Filter bei 450 Hz vor der Zerre
        auto scoopCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 450.0f, 1.3f, juce::Decibels::decibelsToGain(-3.5f));
        for (int ch = 0; ch < 2; ++ch)
            megaMidScoop[ch].coefficients = scoopCoeffs;

        // 1. Pre-Tightening Filters (Steilflankig gegen Matsch)
        preHighpass1.prepare(spec);
        preHighpass1.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        preHighpass2.prepare(spec);
        preHighpass2.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        // Pre-Bite Boost
        for (int ch = 0; ch < 2; ++ch)
            preBiteFilter[ch].prepare(spec);

        // Inter-stage DC Blocker
        for (int stage = 0; stage < 4; ++stage)
        {
            dcBlocker[stage].prepare(spec);
            dcBlocker[stage].setType(juce::dsp::FirstOrderTPTFilterType::highpass);
            dcBlocker[stage].setCutoffFrequency(75.0f);
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

        // Dual Cab Convolvers
        cabLoaderA.prepare(spec);
        cabLoaderB.prepare(spec);

        // Noise Gate Koeffizient (~35 ms Release)
        gateReleaseCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(0.035 * sampleRate));

        updatePreFilters();
        updateToneStack();
    }

    void reset()
    {
        tsHighpass.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            tsMidHump[ch].reset();
            megaMidScoop[ch].reset();
        }

        preHighpass1.reset();
        preHighpass2.reset();
        for (int stage = 0; stage < 4; ++stage)
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

        cabLoaderA.reset();
        cabLoaderB.reset();

        gateEnv = 0.0f;
        gateGain = 1.0f;
    }

    void setGain(float val)             { gainParam = val; }
    void setMaster(float val)           { masterParam = val; }
    void setTight(float val)            { tightParam = val; updatePreFilters(); }
    void setBass(float val)             { bassParam = val; updateToneStack(); }
    void setMiddle(float val)           { midParam = val; updateToneStack(); }
    void setTreble(float val)           { trebleParam = val; updateToneStack(); }
    void setPresence(float val)         { presenceParam = val; updateToneStack(); }
    void setDeep(float val)             { deepParam = val; updateToneStack(); }
    void setMode(int modeIdx)           { ampMode = modeIdx; updatePreFilters(); updateToneStack(); }
    void setTsBoost(bool active)        { tsBoost = active; }
    void setIrBypass(bool bp)           { irBypass = bp; }
    void setIrBlend(float val)          { irBlend = juce::jlimit(0.0f, 1.0f, val); }

    void setGate(float val)
    {
        gateParam = val;
        if (gateParam <= 0.02f)
            gateThreshold = 0.0f; // Gate aus
        else
            gateThreshold = juce::Decibels::decibelsToGain(juce::jmap(gateParam, 0.02f, 1.0f, -80.0f, -22.0f));
    }

    void loadCabinetIR_A(const juce::File& file)
    {
        cabLoaderA.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0);
        hasIrA = true;
    }

    void loadCabinetIR_B(const juce::File& file)
    {
        cabLoaderB.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0);
        hasIrB = true;
    }

    template <typename ProcessContext>
    void process(ProcessContext& context)
    {
        auto& block = context.getOutputBlock();
        const size_t numChannels = block.getNumChannels();
        const size_t numSamples = block.getNumSamples();

        // Ausgewogene Gain-Skalierung für strafferen, musikalischeren Sound
        float driveMultiplier = 1.0f;
        if (ampMode == 0)      driveMultiplier = 2.5f;   // Clean
        else if (ampMode == 1) driveMultiplier = 10.5f;  // Crunch (harmonisch & tight)
        else                   driveMultiplier = 18.0f;  // Mega (nicht matschig, sondern fokussiert)

        const float drive = 1.0f + std::pow(gainParam * driveMultiplier, 1.85f);
        const float outVol = std::pow(masterParam, 1.6f) * 0.75f;

        for (size_t i = 0; i < numSamples; ++i)
        {
            // Schnelle Noise Gate Hysterese auf dem sauberen Gitarren-Eingangssignal
            if (gateThreshold > 0.00001f)
            {
                float inLevel = 0.0f;
                for (size_t ch = 0; ch < numChannels; ++ch)
                    inLevel = std::max(inLevel, std::abs(block.getChannelPointer(ch)[i]));

                if (inLevel > gateEnv)
                    gateEnv = inLevel; // Sofortiges Öffnen beim Anschlag
                else
                    gateEnv += (inLevel - gateEnv) * gateReleaseCoeff;

                float targetGate = (gateEnv >= gateThreshold) ? 1.0f : 0.0f;
                gateGain += (targetGate - gateGain) * (targetGate > gateGain ? 0.08f : 0.003f);
            }
            else
            {
                gateGain = 1.0f;
            }

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float x = data[i] * gateGain; // Gate angewendet
                int fCh = (ch < 2) ? static_cast<int>(ch) : 0;

                // 0. Integrierter Tube Screamer TS808 Boost
                if (tsBoost)
                {
                    float tsSig = tsHighpass.processSample(static_cast<int>(ch), x);
                    tsSig = tsMidHump[fCh].processSample(tsSig);
                    x = std::tanh(tsSig * 2.8f) * 1.65f;
                }

                // 1. Ultra-Tight Pre-Filtering
                x = preHighpass1.processSample(static_cast<int>(ch), x);
                x = preHighpass2.processSample(static_cast<int>(ch), x);
                x = preBiteFilter[fCh].processSample(x);

                // Spezielles Mega Voicing: Vor der Zerre boxige Frequenzen absenken
                if (ampMode == 2)
                    x = megaMidScoop[fCh].processSample(x);

                // 2. Kaskadierte 12AX7-Röhrenstufen
                // Stufe 1: Dynamischer Eingang
                x = triodeStage(x * drive * 0.95f, 1.35f);
                x = dcBlocker[0].processSample(static_cast<int>(ch), x);

                // Stufe 2: Crunch-Sättigungsstufe
                x = triodeStage(x * (1.6f + gainParam * 1.2f), 1.55f);
                x = dcBlocker[1].processSample(static_cast<int>(ch), x);

                // Stufe 3: High-Gain Kompression (Crunch & Mega)
                if (ampMode >= 1)
                {
                    float stage3Drive = (ampMode == 2) ? 2.2f : 1.6f;
                    x = triodeStage(x * stage3Drive, 1.75f);
                    x = dcBlocker[2].processSample(static_cast<int>(ch), x);
                }

                // Stufe 4: Fokussierte Mega-Stufe mit warmer Röhrenkompression
                if (ampMode == 2)
                {
                    x = triodeStage(x * 1.9f, 1.85f);
                    x = dcBlocker[3].processSample(static_cast<int>(ch), x);
                }

                // 3. Tonestack & Endstufen-Deep
                x = bassFilter[fCh].processSample(x);
                x = midFilter[fCh].processSample(x);
                x = trebleFilter[fCh].processSample(x);
                x = presenceFilter[fCh].processSample(x);
                x = deepFilter[fCh].processSample(x);

                // 4. Master Volume
                data[i] = x * outVol;
            }
        }

        // Dual IR Convolver mit Blend
        if (!irBypass)
        {
            if (hasIrA && hasIrB)
            {
                tempBufferB.setSize(static_cast<int>(numChannels), static_cast<int>(numSamples), false, false, true);
                for (size_t ch = 0; ch < numChannels; ++ch)
                    tempBufferB.copyFrom(static_cast<int>(ch), 0, block.getChannelPointer(ch), static_cast<int>(numSamples));

                juce::dsp::AudioBlock<float> blockB(tempBufferB);
                juce::dsp::ProcessContextReplacing<float> contextB(blockB);

                cabLoaderA.process(context);
                cabLoaderB.process(contextB);

                const float weightA = 1.0f - irBlend;
                const float weightB = irBlend;

                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    float* dataA = block.getChannelPointer(ch);
                    const float* dataB = blockB.getChannelPointer(ch);
                    for (size_t i = 0; i < numSamples; ++i)
                        dataA[i] = dataA[i] * weightA + dataB[i] * weightB;
                }
            }
            else if (hasIrA)
            {
                cabLoaderA.process(context);
            }
            else if (hasIrB)
            {
                cabLoaderB.process(context);
            }
        }
    }

private:
    void updatePreFilters()
    {
        if (sampleRate <= 0.0) return;

        float baseFreq = 70.0f;
        if (ampMode == 1) baseFreq = 115.0f;
        if (ampMode == 2) baseFreq = 150.0f;

        float targetCutoff = juce::jmap(tightParam, 0.0f, 1.0f, baseFreq, baseFreq + 120.0f);
        preHighpass1.setCutoffFrequency(targetCutoff);
        preHighpass2.setCutoffFrequency(targetCutoff * 0.75f);

        // Pick-Attack Peak
        float biteFreq = (ampMode == 2) ? 2400.0f : 2100.0f;
        auto biteCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, biteFreq, 1.0f, juce::Decibels::decibelsToGain(juce::jmap(tightParam, 0.0f, 1.0f, 1.0f, 4.5f)));

        for (int ch = 0; ch < 2; ++ch)
            preBiteFilter[ch].coefficients = biteCoeffs;
    }

    void updateToneStack()
    {
        if (sampleRate <= 0.0) return;

        // Bass Low-Shelf (90 Hz)
        auto bassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 90.0f, 0.707f, juce::Decibels::decibelsToGain((bassParam - 0.5f) * 22.0f));

        // Mittenfrequenz: Im Mega-Modus 620 Hz für mächtiges Diezel-Knurren, sonst 680 Hz
        float midCenter = (ampMode == 2) ? 620.0f : 680.0f;
        auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, midCenter, 1.2f, juce::Decibels::decibelsToGain((midParam - 0.5f) * 22.0f));

        // Treble High-Shelf (3.6 kHz)
        auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 3600.0f, 0.707f, juce::Decibels::decibelsToGain((trebleParam - 0.5f) * 20.0f));

        // Presence Peaking (5.2 kHz)
        auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 5200.0f, 0.8f, juce::Decibels::decibelsToGain((presenceParam - 0.5f) * 16.0f));

        // DIEZEL DEEP: Resonanzpeak bei 56 Hz
        auto deepCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 56.0f, 1.7f, juce::Decibels::decibelsToGain(deepParam * 18.0f));

        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].coefficients = bassCoeffs;
            midFilter[ch].coefficients = midCoeffs;
            trebleFilter[ch].coefficients = trebleCoeffs;
            presenceFilter[ch].coefficients = presenceCoeffs;
            deepFilter[ch].coefficients = deepCoeffs;
        }
    }

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
    float tightParam    = 0.6f;
    float bassParam     = 0.5f;
    float midParam      = 0.55f;
    float trebleParam   = 0.5f;
    float presenceParam = 0.5f;
    float deepParam     = 0.65f;
    int   ampMode       = 2;
    bool  tsBoost       = false;
    bool  irBypass      = false;

    // Dual IR Loader
    float irBlend       = 0.5f;
    bool  hasIrA        = false;
    bool  hasIrB        = false;
    juce::AudioBuffer<float> tempBufferB;
    juce::dsp::Convolution cabLoaderA;
    juce::dsp::Convolution cabLoaderB;

    // Noise Gate
    float gateParam     = 0.4f;
    float gateThreshold = 0.0f;
    float gateEnv       = 0.0f;
    float gateGain      = 1.0f;
    float gateReleaseCoeff = 0.01f;

    juce::dsp::FirstOrderTPTFilter<float> tsHighpass;
    juce::dsp::IIR::Filter<float> tsMidHump[2];
    juce::dsp::IIR::Filter<float> megaMidScoop[2];

    juce::dsp::FirstOrderTPTFilter<float> preHighpass1;
    juce::dsp::FirstOrderTPTFilter<float> preHighpass2;
    juce::dsp::IIR::Filter<float> preBiteFilter[2];
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker[4];

    juce::dsp::IIR::Filter<float> bassFilter[2];
    juce::dsp::IIR::Filter<float> midFilter[2];
    juce::dsp::IIR::Filter<float> trebleFilter[2];
    juce::dsp::IIR::Filter<float> presenceFilter[2];
    juce::dsp::IIR::Filter<float> deepFilter[2];
};
