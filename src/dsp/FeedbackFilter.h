#pragma once
#include <cmath>

// Bypassable one-pole HP+LP filter pair for feedback bus.
// Dual-mono processing (up to 2 channels). Signal chain: HP first, then LP.
// Header-only, JUCE-free.
class FeedbackFilter
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        recalcLP();
        recalcHP();
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            lpState[ch] = 0.0f;
            hpLpState[ch] = 0.0f;  // LP state used for HP subtraction method
        }
    }

    void setLPFrequency(float freqHz)
    {
        lpFreq = freqHz;
        recalcLP();
    }

    void setHPFrequency(float freqHz)
    {
        hpFreq = freqHz;
        recalcHP();
    }

    void setLPBypassed(bool bypassed) { lpBypassed = bypassed; }
    void setHPBypassed(bool bypassed) { hpBypassed = bypassed; }

    float process(int channel, float input)
    {
        if (channel < 0 || channel >= maxChannels)
            return input;

        float signal = input;

        // HP first (subtraction method: hp_out = input - lowpass(input))
        if (!hpBypassed)
        {
            hpLpState[channel] += hpAlpha * (signal - hpLpState[channel]);
            // Denormal protection
            if (std::abs(hpLpState[channel]) < 1e-15f)
                hpLpState[channel] = 0.0f;
            signal = signal - hpLpState[channel];
        }

        // LP second
        if (!lpBypassed)
        {
            lpState[channel] += lpAlpha * (signal - lpState[channel]);
            // Denormal protection
            if (std::abs(lpState[channel]) < 1e-15f)
                lpState[channel] = 0.0f;
            signal = lpState[channel];
        }

        return signal;
    }

private:
    void recalcLP()
    {
        if (sr <= 0.0)
            return;
        lpAlpha = 1.0f - std::exp(static_cast<float>(-6.283185307179586 * lpFreq / sr));
    }

    void recalcHP()
    {
        if (sr <= 0.0)
            return;
        hpAlpha = 1.0f - std::exp(static_cast<float>(-6.283185307179586 * hpFreq / sr));
    }

    static constexpr int maxChannels = 2;
    double sr = 44100.0;

    // LP filter
    float lpFreq = 20000.0f;
    float lpAlpha = 1.0f;
    float lpState[maxChannels] = {};
    bool lpBypassed = true;

    // HP filter (uses internal LP for subtraction method)
    float hpFreq = 20.0f;
    float hpAlpha = 0.0f;
    float hpLpState[maxChannels] = {};
    bool hpBypassed = true;
};
