#pragma once
#include <juce_dsp/juce_dsp.h>

class CharacterProcessor
{
public:
    void prepare(double sampleRate, int maxChannels)
    {
        sr = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = 512;
        spec.numChannels = static_cast<juce::uint32>(maxChannels);

        for (int ch = 0; ch < maxChannels && ch < maxCh; ++ch)
        {
            lpFilter[ch].reset();
            lpFilter[ch].prepare({sampleRate, 512, 1});
            lpFilter[ch].setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
            lpFilter[ch].setCutoffFrequency(20000.0f);
        }

        numChannels = maxChannels < maxCh ? maxChannels : maxCh;
        random.setSeed(42);
    }

    float process(int channel, float sample, float characterAmount)
    {
        if (channel < 0 || channel >= numChannels)
            return sample;

        // Interpolate cutoff: 20kHz (clean) to ~4kHz (full BBD)
        float cutoff = 20000.0f - characterAmount * 16000.0f;
        cutoff = std::max(cutoff, 200.0f);
        lpFilter[channel].setCutoffFrequency(cutoff);

        float filtered = lpFilter[channel].processSample(0, sample);

        // Add subtle noise floor scaled by character
        constexpr float noiseLevel = 0.0005f;
        float noise = (random.nextFloat() * 2.0f - 1.0f) * characterAmount * noiseLevel;

        return filtered + noise;
    }

    void reset()
    {
        for (int ch = 0; ch < numChannels; ++ch)
            lpFilter[ch].reset();
    }

private:
    static constexpr int maxCh = 2;
    juce::dsp::FirstOrderTPTFilter<float> lpFilter[2];
    juce::Random random;
    double sr = 44100.0;
    int numChannels = 0;
};
