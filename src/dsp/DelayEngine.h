#pragma once
#include "TapReader.h"
#include "CharacterProcessor.h"
#include "OnePoleSmooth.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>

class DelayEngine
{
public:
    static constexpr int numTaps = 8;
    static constexpr float maxBaseDelayMs = 150.0f;
    static constexpr float maxMultiplier = 33.0f;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        blockSize = maxBlockSize;

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

        // Smoothing times: longer for delay-time parameters to avoid
        // read-pointer discontinuities that cause audible clicks
        baseDelaySmoother.setSampleRate(sampleRate);
        baseDelaySmoother.setTimeMs(50.0f);
        multiplierSmoother.setSampleRate(sampleRate);
        multiplierSmoother.setTimeMs(50.0f);
        characterSmoother.setSampleRate(sampleRate);
        characterSmoother.setTimeMs(10.0f);

        // Allocate scratch buffers for per-sample smoothed values
        smoothedBaseDelay.resize(static_cast<size_t>(maxBlockSize));
        smoothedMultiplier.resize(static_cast<size_t>(maxBlockSize));
        smoothedCharacter.resize(static_cast<size_t>(maxBlockSize));

        for (int t = 0; t < numTaps; ++t)
        {
            smoothedTapDelay[t].resize(static_cast<size_t>(maxBlockSize));
            smoothedTapLevel[t].resize(static_cast<size_t>(maxBlockSize));
        }
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

        // Ensure scratch buffers are large enough (handles host block size changes)
        if (static_cast<size_t>(numSamples) > smoothedBaseDelay.size())
        {
            auto sz = static_cast<size_t>(numSamples);
            smoothedBaseDelay.resize(sz);
            smoothedMultiplier.resize(sz);
            smoothedCharacter.resize(sz);
            for (int t = 0; t < numTaps; ++t)
            {
                smoothedTapDelay[t].resize(sz);
                smoothedTapLevel[t].resize(sz);
            }
        }

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

        // Pre-compute per-sample smoothed values for base delay, multiplier,
        // character, and tap delay/level so they advance exactly once per sample
        // (not once per channel).
        for (size_t i = 0; i < static_cast<size_t>(numSamples); ++i)
        {
            smoothedBaseDelay[i] = baseDelaySmoother.getNextValue();
            smoothedMultiplier[i] = multiplierSmoother.getNextValue();
            smoothedCharacter[i] = characterSmoother.getNextValue();

            for (int t = 0; t < numTaps; ++t)
            {
                smoothedTapDelay[t][i] = taps[t].getDelaySamples(
                    smoothedBaseDelay[i], smoothedMultiplier[i], sr, quantize);
                smoothedTapLevel[t][i] = taps[t].getLevel();
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer(ch);

            for (size_t i = 0; i < static_cast<size_t>(numSamples); ++i)
            {
                // Apply character to input before pushing to delay line
                float processed = characterProcessor.process(ch, channelData[i],
                                                              smoothedCharacter[i]);
                delayLine[ch].pushSample(0, processed);

                // Sum taps -- last tap updates read pointer
                float wetSample = 0.0f;
                for (int t = 0; t < numTaps; ++t)
                {
                    bool isLastTap = (t == numTaps - 1);
                    float tapOut = delayLine[ch].popSample(0,
                        smoothedTapDelay[t][i], isLastTap);
                    wetSample += tapOut * smoothedTapLevel[t][i];
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

    // Pre-allocated scratch buffers for per-sample smoothed values
    // (allocated in prepare, read in process -- no audio-thread allocation)
    std::vector<float> smoothedBaseDelay;
    std::vector<float> smoothedMultiplier;
    std::vector<float> smoothedCharacter;
    std::vector<float> smoothedTapDelay[numTaps];
    std::vector<float> smoothedTapLevel[numTaps];

    double sr = 44100.0;
    int blockSize = 512;
};
