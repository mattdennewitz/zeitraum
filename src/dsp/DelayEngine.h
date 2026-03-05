#pragma once
#include "TapReader.h"
#include "CharacterProcessor.h"
#include "OnePoleSmooth.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>

class DelayEngine
{
public:
    static constexpr int numTaps = 8;
    static constexpr float maxBaseDelayMs = 150.0f;
    static constexpr float maxMultiplier = 33.0f;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        // Max delay: 150ms * 33 * sampleRate
        int maxDelaySamples = static_cast<int>(std::ceil(
            maxBaseDelayMs * maxMultiplier * 0.001 * sampleRate)) + 1;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
        spec.numChannels = 1;

        for (int ch = 0; ch < 2; ++ch)
        {
            delayLine[ch].reset();
            delayLine[ch].prepare(spec);
            delayLine[ch].setMaximumDelayInSamples(maxDelaySamples);
        }

        for (int t = 0; t < numTaps; ++t)
        {
            taps[t].prepare(sampleRate);
            taps[t].resetToDefault(t);
        }

        characterProcessor.prepare(sampleRate, 2);

        baseDelaySmoother.setSampleRate(sampleRate);
        baseDelaySmoother.setTimeMs(10.0f);
        multiplierSmoother.setSampleRate(sampleRate);
        multiplierSmoother.setTimeMs(10.0f);
        characterSmoother.setSampleRate(sampleRate);
        characterSmoother.setTimeMs(10.0f);
    }

    void process(juce::AudioBuffer<float>& buffer,
                 float baseDelayMs,
                 float multiplier,
                 float characterAmount,
                 bool quantize,
                 const float* tapPositions,
                 const float* tapLevels)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min(buffer.getNumChannels(), 2);

        // Update smoother targets
        baseDelaySmoother.setTargetValue(baseDelayMs);
        multiplierSmoother.setTargetValue(multiplier);
        characterSmoother.setTargetValue(characterAmount);

        // Update tap positions and levels
        for (int t = 0; t < numTaps; ++t)
        {
            taps[t].setPosition(tapPositions[t]);
            taps[t].setLevel(tapLevels[t]);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float baseMs = baseDelaySmoother.getNextValue();
                float mult = multiplierSmoother.getNextValue();
                float character = characterSmoother.getNextValue();

                // Apply character to input before pushing to delay line
                float processed = characterProcessor.process(ch, channelData[i], character);
                delayLine[ch].pushSample(0, processed);

                // Sum taps -- last tap updates read pointer
                float wetSample = 0.0f;
                for (int t = 0; t < numTaps; ++t)
                {
                    float delaySamples = taps[t].getDelaySamples(baseMs, mult, sr, quantize);
                    float level = taps[t].getLevel();
                    bool isLastTap = (t == numTaps - 1);
                    float tapOut = delayLine[ch].popSample(0, delaySamples, isLastTap);
                    wetSample += tapOut * level;
                }

                channelData[i] = wetSample;
            }
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            delayLine[ch].reset();

        characterProcessor.reset();

        baseDelaySmoother.reset(0.0f);
        multiplierSmoother.reset(1.0f);
        characterSmoother.reset(0.0f);

        for (int t = 0; t < numTaps; ++t)
            taps[t].resetToDefault(t);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine[2];
    TapReader taps[numTaps];
    CharacterProcessor characterProcessor;

    OnePoleSmooth baseDelaySmoother;
    OnePoleSmooth multiplierSmoother;
    OnePoleSmooth characterSmoother;

    double sr = 44100.0;
};
