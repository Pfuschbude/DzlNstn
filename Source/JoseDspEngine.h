#pragma once
#include <juce_dsp/juce_dsp.h>

class JoseDspEngine
{
public:
    JoseDspEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Pre-Filters
        preTightFilter.prepare(spec);
        preTightFilter.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        brightShelf.prepare(spec);
        brightShelf.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

        // Tone Stack Filters (Biquad IIR pro Kanal)
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].prepare(spec);
            midFilter[ch].prepare(spec);
            trebleFilter[ch].prepare(spec);
            presenceFilter[ch].prepare(spec);
        }

        // Cab IR Convolver
        cabLoader.prepare(spec);

        updatePreFilters();
        updateToneStack();
    }

    void reset()
    {
        preTightFilter.reset();
        brightShelf.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].reset();
            midFilter[ch].reset();
            trebleFilter[ch].reset();
            presenceFilter[ch].reset();
        }
        cabLoader.reset();
    }

    // --- MODDING PARAMETER SETTERS ---

    // 1. V1b Cathode Cap Mod (0.47 uF bis 22.0 uF)
    void setCathodeCapValue(float capMicroFarads)
    {
        cathodeCapUf = capMicroFarads;
        updatePreFilters();
    }

    // 2. Bright Cap Mod (0 pF bis 4700 pF)
    void setBrightCapValue(float capPicoFarads)
    {
        brightCapPf = capPicoFarads;
        updatePreFilters();
    }

    // 3. Tone Stack Slope Resistor Mod (22k bis 56k)
    void setSlopeResistor(float ohms)
    {
        slopeOhms = ohms;
        updateToneStack();
    }

    // 4. Diode Clipping Typ: 0=Bypass(Stock), 1=Silicon(1N4148), 2=Zener 4.7V (Jose Mod), 3=LED
    void setDiodeMode(int mode)
    {
        diodeMode = mode;
    }

    // Standard Amp Controls
    void setPreampGain(float gainNorm)       { gainParam = gainNorm; }
    void setMasterVolume(float masterNorm)   { masterParam = masterNorm; }
    void setBass(float val)                  { bassParam = val; updateToneStack(); }
    void setMiddle(float val)                { midParam = val; updateToneStack(); }
    void setTreble(float val)                { trebleParam = val; updateToneStack(); }
    void setPresence(float val)              { presenceParam = val; updateToneStack(); }
    void setIrBypass(bool bypass)            { irBypass = bypass; }

    void loadCabinetIR(const juce::File& file)
    {
        cabLoader.loadImpulseResponse(
            file,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            0
        );
        hasCustomIr = true;
    }

    template <typename ProcessContext>
    void process(ProcessContext& context)
    {
        auto& block = context.getOutputBlock();
        const size_t numChannels = block.getNumChannels();
        const size_t numSamples = block.getNumSamples();

        const float drive = 1.0f + std::pow(gainParam * 7.5f, 2.3f);
        const float outVol = std::pow(masterParam, 1.8f) * 0.8f;

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float x = data[i];

                // 1. Cathode V1b Bass Tightening Filter
                x = preTightFilter.processSample(static_cast<int>(ch), x);

                // 2. Bright Cap Boost abhängig von Gain
                if (brightCapPf > 10.0f && gainParam < 0.85f)
                {
                    float brightAmount = (1.0f - gainParam) * (brightCapPf / 2000.0f);
                    x += brightShelf.processSample(static_cast<int>(ch), x) * brightAmount;
                }

                // 3. Preamp Gain
                x *= drive;

                // 4. Triode Saturation + Jose Dioden Clipping
                x = processClipping(x);

                // 5. Tone Stack
                int filterChannel = (ch < 2) ? static_cast<int>(ch) : 0;
                x = bassFilter[filterChannel].processSample(x);
                x = midFilter[filterChannel].processSample(x);
                x = trebleFilter[filterChannel].processSample(x);
                x = presenceFilter[filterChannel].processSample(x);

                // 6. Master Volume
                data[i] = x * outVol;
            }
        }

        // 7. IR Loader Convolution
        if (!irBypass && hasCustomIr)
        {
            cabLoader.process(context);
        }
    }

private:
    void updatePreFilters()
    {
        if (sampleRate <= 0.0) return;

        // fc = 1 / (2 * pi * Rk * Ck) mit Marshall Rk = 2.7k Ohm
        const float rk = 2700.0f;
        const float ck = cathodeCapUf * 1e-6f;
        float cutoff = 1.0f / (2.0f * juce::MathConstants<float>::pi * rk * ck);
        preTightFilter.setCutoffFrequency(juce::jlimit(20.0f, 2000.0f, cutoff));

        // Bright Cap Highpass Grenzfrequenz (~2.5 kHz)
        brightShelf.setCutoffFrequency(2500.0f);
    }

    void updateToneStack()
    {
        if (sampleRate <= 0.0) return;

        auto bassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 110.0f, 0.707f, juce::Decibels::decibelsToGain((bassParam - 0.5f) * 24.0f));

        float midCenterFreq = juce::jmap(slopeOhms, 22000.0f, 56000.0f, 600.0f, 950.0f);
        auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, midCenterFreq, 1.2f, juce::Decibels::decibelsToGain((midParam - 0.5f) * 20.0f));

        auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 3200.0f, 0.707f, juce::Decibels::decibelsToGain((trebleParam - 0.5f) * 22.0f));

        auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, 4800.0f, 0.8f, juce::Decibels::decibelsToGain((presenceParam - 0.5f) * 16.0f));

        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].coefficients = bassCoeffs;
            midFilter[ch].coefficients = midCoeffs;
            trebleFilter[ch].coefficients = trebleCoeffs;
            presenceFilter[ch].coefficients = presenceCoeffs;
        }
    }

    inline float processClipping(float s)
    {
        s = std::tanh(s * 1.3f);

        switch (diodeMode)
        {
            case 1: // Silicon Diodes (1N4148)
            {
                const float thresh = 0.55f;
                if (s > thresh)  s = thresh + 0.08f * std::tanh((s - thresh) * 4.0f);
                if (s < -thresh) s = -thresh + 0.08f * std::tanh((s + thresh) * 4.0f);
                break;
            }
            case 2: // Jose Mod (Asymmetrische Zener 4.7V)
            {
                const float vPos = 0.65f;
                const float vNeg = -0.38f;
                if (s > vPos)  s = vPos + 0.12f * std::tanh((s - vPos) * 3.5f);
                if (s < vNeg) s = vNeg + 0.09f * std::tanh((s - vNeg) * 4.5f);
                s *= 1.4f;
                break;
            }
            case 3: // LEDs
            {
                const float thresh = 0.95f;
                if (s > thresh)  s = thresh + 0.15f * std::tanh((s - thresh) * 2.5f);
                if (s < -thresh) s = -thresh + 0.15f * std::tanh((s + thresh) * 2.5f);
                break;
            }
            default:
                break;
        }

        return s;
    }

    double sampleRate = 48000.0;

    float cathodeCapUf = 0.68f;
    float brightCapPf  = 1000.0f;
    float slopeOhms    = 33000.0f;
    int   diodeMode    = 2;

    float gainParam     = 0.8f;
    float masterParam   = 0.6f;
    float bassParam     = 0.5f;
    float midParam      = 0.65f;
    float trebleParam   = 0.7f;
    float presenceParam = 0.6f;
    bool  irBypass      = false;
    bool  hasCustomIr   = false;

    juce::dsp::FirstOrderTPTFilter<float> preTightFilter;
    juce::dsp::FirstOrderTPTFilter<float> brightShelf;

    // Diskrete IIR Filter pro Kanal (Stereo-fähig ohne Duplicator-Methoden-Konflikt)
    juce::dsp::IIR::Filter<float> bassFilter[2];
    juce::dsp::IIR::Filter<float> midFilter[2];
    juce::dsp::IIR::Filter<float> trebleFilter[2];
    juce::dsp::IIR::Filter<float> presenceFilter[2];

    juce::dsp::Convolution cabLoader;
};
