#pragma once
#include "OnePoleSmooth.h"
#include <array>
#include <vector>

class FeedbackMatrix
{
public:
    static constexpr int numTaps = 8;
    static constexpr int numPresetMixes = 4; // Odd, Even, Rising, Falling
    static constexpr int numSources = numTaps + numPresetMixes; // 12

    // Fixed preset mix weights (normalized so each sums to 1.0)
    static constexpr std::array<float, numTaps> oddWeights     = {0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f};
    static constexpr std::array<float, numTaps> evenWeights    = {0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f};
    static constexpr std::array<float, numTaps> risingWeights  = {1.0f/36.0f, 2.0f/36.0f, 3.0f/36.0f, 4.0f/36.0f,
                                                                   5.0f/36.0f, 6.0f/36.0f, 7.0f/36.0f, 8.0f/36.0f};
    static constexpr std::array<float, numTaps> fallingWeights = {8.0f/36.0f, 7.0f/36.0f, 6.0f/36.0f, 5.0f/36.0f,
                                                                   4.0f/36.0f, 3.0f/36.0f, 2.0f/36.0f, 1.0f/36.0f};

    void prepare(double sampleRate, int maxBlockSize)
    {
        for (int i = 0; i < numSources; ++i)
        {
            smoothers[i].setSampleRate(sampleRate);
            smoothers[i].setTimeMs(7.0f);
            smoothers[i].reset(0.0f);
        }

        // Allocate scratch buffers for pre-computed smoothed gains
        for (int i = 0; i < numSources; ++i)
            smoothedGains[i].resize(static_cast<size_t>(maxBlockSize), 0.0f);

        presetWeights[0] = &oddWeights;
        presetWeights[1] = &evenWeights;
        presetWeights[2] = &risingWeights;
        presetWeights[3] = &fallingWeights;
    }

    void reset()
    {
        for (int i = 0; i < numSources; ++i)
            smoothers[i].reset(0.0f);
    }

    void setSourceGain(int sourceIndex, float gain)
    {
        if (sourceIndex >= 0 && sourceIndex < numSources)
            smoothers[sourceIndex].setTargetValue(gain);
    }

    // Pre-compute smoothed gain values for the block.
    // Call once before the per-channel loop to avoid double-advancing smoothers.
    void prepareSmoothGains(int numSamples)
    {
        for (int src = 0; src < numSources; ++src)
        {
            for (int i = 0; i < numSamples; ++i)
                smoothedGains[src][static_cast<size_t>(i)] = smoothers[src].getNextValue();
        }
    }

    // Compute feedback bus value from tap outputs for one sample.
    // tapOutputs: array of 8 float values (current sample's tap outputs)
    // sampleIndex: index into pre-computed smoothed gain buffers
    float process(const float* tapOutputs, int sampleIndex)
    {
        float bus = 0.0f;

        // Individual tap feedback
        for (int t = 0; t < numTaps; ++t)
            bus += tapOutputs[t] * smoothedGains[t][static_cast<size_t>(sampleIndex)];

        // Preset mix feedback
        for (int m = 0; m < numPresetMixes; ++m)
        {
            float mixSum = 0.0f;
            const auto& weights = *presetWeights[m];
            for (int t = 0; t < numTaps; ++t)
                mixSum += tapOutputs[t] * weights[static_cast<size_t>(t)];

            bus += mixSum * smoothedGains[numTaps + m][static_cast<size_t>(sampleIndex)];
        }

        return bus;
    }

private:
    std::array<OnePoleSmooth, numSources> smoothers;
    std::array<std::vector<float>, numSources> smoothedGains;
    std::array<const std::array<float, numTaps>*, numPresetMixes> presetWeights = {};
};
