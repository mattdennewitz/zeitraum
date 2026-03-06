#pragma once
#include <cmath>
#include <algorithm>

// tanh soft clip + RMS energy limiter for feedback bus.
// Header-only, JUCE-free.
// Usage: call updateRms(L, R) once per sample, then process() for each channel.
class FeedbackSaturator
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;

        // RMS follower: ~50ms window
        rmsAlpha = 1.0f - std::exp(static_cast<float>(-6.283185307179586 / (0.05 * sampleRate)));

        // Gain smoothing: attack ~5ms, release ~200ms
        attackAlpha = 1.0f - std::exp(static_cast<float>(-6.283185307179586 / (0.005 * sampleRate)));
        releaseAlpha = 1.0f - std::exp(static_cast<float>(-6.283185307179586 / (0.2 * sampleRate)));

        reset();
    }

    void reset()
    {
        rmsSquared = 0.0f;
        currentGain = 1.0f;
    }

    // Call once per sample before process() for stereo linking.
    // Uses max(L^2, R^2) for stereo-linked RMS.
    void updateRms(float inputL, float inputR)
    {
        float maxSquared = std::max(inputL * inputL, inputR * inputR);
        rmsSquared += rmsAlpha * (maxSquared - rmsSquared);

        // Denormal protection
        if (rmsSquared < 1e-15f)
            rmsSquared = 0.0f;

        // Compute target gain from RMS
        float rms = std::sqrt(rmsSquared);
        float targetGain = (rms > threshold) ? (threshold / rms) : 1.0f;

        // Smooth gain: fast attack, slow release
        float alpha = (targetGain < currentGain) ? attackAlpha : releaseAlpha;
        currentGain += alpha * (targetGain - currentGain);
    }

    // Apply soft clip then energy limiter gain.
    float process(float input)
    {
        // Stage 1: tanh soft clip
        // tanh(x) ~ x for small signals (unity gain), bounded to [-1,+1] for large
        float clipped = std::tanh(input);

        // Stage 2: energy limiter gain
        return clipped * currentGain;
    }

private:
    double sr = 44100.0;

    // RMS energy limiter
    float rmsAlpha = 0.0f;
    float attackAlpha = 0.0f;
    float releaseAlpha = 0.0f;
    float rmsSquared = 0.0f;
    float currentGain = 1.0f;
    static constexpr float threshold = 0.85f;
};
