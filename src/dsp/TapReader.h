#pragma once
#include "OnePoleSmooth.h"
#include <algorithm>
#include <cmath>

class TapReader
{
public:
    static float defaultPosition(int tapIndex)
    {
        return static_cast<float>(tapIndex + 1) / 8.0f;
    }

    void prepare(double sampleRate)
    {
        positionSmoother.setSampleRate(sampleRate);
        positionSmoother.setTimeMs(10.0f);

        levelSmoother.setSampleRate(sampleRate);
        levelSmoother.setTimeMs(5.0f);
    }

    void setPosition(float pos)
    {
        positionSmoother.setTargetValue(pos);
    }

    void setLevel(float lvl)
    {
        levelSmoother.setTargetValue(lvl);
    }

    float getDelaySamples(float baseDelayMs, float multiplier, double sampleRate, bool quantize)
    {
        float smoothedPosition = positionSmoother.getNextValue();
        float delayMs = smoothedPosition * baseDelayMs * multiplier;

        if (quantize)
        {
            delayMs = std::round(delayMs / 10.0f) * 10.0f;
            delayMs = std::max(delayMs, 10.0f);
        }

        float delaySamples = delayMs * 0.001f * static_cast<float>(sampleRate);
        return std::max(delaySamples, 1.0f);
    }

    float getLevel()
    {
        return levelSmoother.getNextValue();
    }

    void resetToDefault(int tapIndex)
    {
        positionSmoother.reset(defaultPosition(tapIndex));
        levelSmoother.reset(1.0f);
    }

private:
    OnePoleSmooth positionSmoother;
    OnePoleSmooth levelSmoother;
};
