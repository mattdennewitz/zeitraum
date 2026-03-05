#pragma once
#include <cmath>

class OnePoleSmooth
{
public:
    void setTargetValue(float newTarget) { target = newTarget; }

    void reset(float value) { current = target = value; }

    void setSampleRate(double sr)
    {
        sampleRate = sr;
        recalcAlpha();
    }

    void setTimeMs(float ms)
    {
        timeMs = ms;
        recalcAlpha();
    }

    float getNextValue()
    {
        current += alpha * (target - current);
        return current;
    }

    bool isSmoothing() const
    {
        return std::abs(target - current) > 1e-6f;
    }

private:
    void recalcAlpha()
    {
        if (timeMs <= 0.0f || sampleRate <= 0.0)
        {
            alpha = 1.0f;
            return;
        }
        alpha = 1.0f - std::exp(-6.2831853f / (timeMs * 0.001f * static_cast<float>(sampleRate)));
    }

    float current = 0.0f;
    float target = 0.0f;
    float alpha = 1.0f;
    float timeMs = 10.0f;
    double sampleRate = 44100.0;
};
