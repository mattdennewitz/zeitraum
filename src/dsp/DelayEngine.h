#pragma once
#include "TapReader.h"
#include "CharacterProcessor.h"
#include "OnePoleSmooth.h"
#include "FeedbackMatrix.h"
#include "FeedbackFilter.h"
#include "FeedbackSaturator.h"
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

        // Feedback components
        feedbackMatrix.prepare(sampleRate, maxBlockSize);
        feedbackFilter.prepare(sampleRate);
        feedbackSaturator.prepare(sampleRate);

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

    // Full process with feedback parameters
    void process(juce::AudioBuffer<float>& buffer,
                 float baseDelayMs,
                 float multiplier,
                 float characterAmount,
                 bool quantize,
                 const float* tapPositions,
                 const float* tapLevels,
                 const float* feedbackGains,
                 float fbHPFreq, float fbLPFreq,
                 bool fbHPOn, bool fbLPOn)
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

        // Set feedback gains and filter params
        for (int s = 0; s < FeedbackMatrix::numSources; ++s)
            feedbackMatrix.setSourceGain(s, feedbackGains[s]);

        feedbackFilter.setHPFrequency(fbHPFreq);
        feedbackFilter.setLPFrequency(fbLPFreq);
        feedbackFilter.setHPBypassed(!fbHPOn);
        feedbackFilter.setLPBypassed(!fbLPOn);

        // Pre-compute smoothed gains BEFORE channel loop (smoother separation)
        feedbackMatrix.prepareSmoothGains(numSamples);

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

        // Interleaved per-sample processing across channels for proper
        // stereo-linked saturation (both channels' energy at each sample)
        float* channelData[2] = {nullptr, nullptr};
        for (int ch = 0; ch < numChannels; ++ch)
            channelData[ch] = buffer.getWritePointer(ch);

        for (size_t i = 0; i < static_cast<size_t>(numSamples); ++i)
        {
            float fbBusPerCh[2] = {0.0f, 0.0f};
            float inputSamples[2] = {0.0f, 0.0f};

            // Save original input before overwriting with wet output
            for (int ch = 0; ch < numChannels; ++ch)
                inputSamples[ch] = channelData[ch][i];

            // For each channel: pop taps, compute feedback bus, filter
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // (a) Pop all taps FIRST (read from previously-pushed data)
                float tapOutputs[numTaps];
                float wetSample = 0.0f;
                for (int t = 0; t < numTaps; ++t)
                {
                    bool isLastTap = (t == numTaps - 1);
                    tapOutputs[t] = delayLine[ch].popSample(0,
                        smoothedTapDelay[t][i], isLastTap);
                    wetSample += tapOutputs[t] * smoothedTapLevel[t][i];
                }

                // (b) Compute feedback bus from matrix
                float fbBus = feedbackMatrix.process(tapOutputs, static_cast<int>(i));

                // (c) Filter feedback bus
                fbBus = feedbackFilter.process(ch, fbBus);

                fbBusPerCh[ch] = fbBus;
                channelData[ch][i] = wetSample; // write wet output
            }

            // (d) Stereo-linked saturation: update RMS with both channels
            float fbL = fbBusPerCh[0];
            float fbR = (numChannels > 1) ? fbBusPerCh[1] : fbBusPerCh[0];
            feedbackSaturator.updateRms(fbL, fbR);

            // (e-f) For each channel: saturate, apply character, push input+feedback
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float fbBus = feedbackSaturator.process(fbBusPerCh[ch]);

                float processed = characterProcessor.process(ch, inputSamples[ch],
                                                              smoothedCharacter[i]);

                // Push input + feedback to delay line
                delayLine[ch].pushSample(0, processed + fbBus);
            }
        }
    }

    // Backward-compatible overload (no feedback -- all gains zero)
    void process(juce::AudioBuffer<float>& buffer,
                 float baseDelayMs,
                 float multiplier,
                 float characterAmount,
                 bool quantize,
                 const float* tapPositions,
                 const float* tapLevels)
    {
        float zeroGains[FeedbackMatrix::numSources] = {};
        process(buffer, baseDelayMs, multiplier, characterAmount, quantize,
                tapPositions, tapLevels, zeroGains, 20.0f, 20000.0f, false, false);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            delayLine[ch].reset();

        characterProcessor.reset();
        feedbackMatrix.reset();
        feedbackFilter.reset();
        feedbackSaturator.reset();

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
    FeedbackMatrix feedbackMatrix;
    FeedbackFilter feedbackFilter;
    FeedbackSaturator feedbackSaturator;

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
