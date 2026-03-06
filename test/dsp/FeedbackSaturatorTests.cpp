#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/FeedbackSaturator.h"
#include <cmath>
#include <vector>

static constexpr double kSampleRate = 44100.0;

TEST_CASE("FeedbackSaturator small signals near unity gain", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Small signal: amplitude 0.2
    float input = 0.2f;
    sat.updateRms(input, input);
    float output = sat.process(input);

    // Should be within 5% of input
    REQUIRE(output > input * 0.95f);
    REQUIRE(output < input * 1.05f);
}

TEST_CASE("FeedbackSaturator large signals bounded to approx [-1, +1]", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Large positive signal
    sat.updateRms(5.0f, 5.0f);
    float outPos = sat.process(5.0f);
    REQUIRE(outPos <= 1.05f);
    REQUIRE(outPos > 0.0f);

    // Large negative signal
    sat.updateRms(-5.0f, -5.0f);
    float outNeg = sat.process(-5.0f);
    REQUIRE(outNeg >= -1.05f);
    REQUIRE(outNeg < 0.0f);
}

TEST_CASE("FeedbackSaturator soft clip is symmetric", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    float input = 2.0f;
    sat.updateRms(input, input);
    float outPos = sat.process(input);

    FeedbackSaturator sat2;
    sat2.prepare(kSampleRate);
    sat2.updateRms(-input, -input);
    float outNeg = sat2.process(-input);

    REQUIRE(std::abs(outPos + outNeg) < 0.001f);  // Symmetric
}

TEST_CASE("FeedbackSaturator energy limiter reduces sustained loud signals", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Process sustained sine at amplitude 2.0 for enough time to trigger limiter
    const int numSamples = static_cast<int>(kSampleRate * 0.5);  // 500ms
    const double twoPi = 6.283185307179586;
    float lastOutput = 0.0f;
    float peakEarly = 0.0f;
    float peakLate = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 2.0f * static_cast<float>(std::sin(twoPi * 440.0 * i / kSampleRate));
        sat.updateRms(sample, sample);
        float out = sat.process(sample);

        // Track peaks in first 100 samples and last 1000 samples
        if (i < 100)
            peakEarly = std::max(peakEarly, std::abs(out));
        if (i > numSamples - 1000)
            peakLate = std::max(peakLate, std::abs(out));
    }

    // Late peaks should be reduced compared to what pure tanh clipping would give
    // tanh(2.0 * 1.5) / tanh(1.5) ~ 0.997, so without limiter peak would be ~1.0
    // With limiter active, gain should be reduced
    REQUIRE(peakLate < 0.95f);
}

TEST_CASE("FeedbackSaturator transient peaks not immediately squashed", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Process silence first to ensure limiter gain is at 1.0
    for (int i = 0; i < 1000; ++i)
    {
        sat.updateRms(0.0f, 0.0f);
        sat.process(0.0f);
    }

    // Then hit with a loud transient
    float transient = 2.0f;
    sat.updateRms(transient, transient);
    float firstOut = sat.process(transient);

    // The first sample should still be relatively loud (limiter hasn't kicked in yet)
    // tanh(2.0 * 1.5) / tanh(1.5) ~ 0.997 with gain near 1.0
    REQUIRE(firstOut > 0.8f);
}

TEST_CASE("FeedbackSaturator stereo linking: same gain for both channels", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Feed loud signal to build up RMS
    for (int i = 0; i < 22050; ++i)
    {
        float sample = 2.0f * static_cast<float>(std::sin(6.283185 * 440.0 * i / kSampleRate));
        sat.updateRms(sample, sample);
        sat.process(sample);
    }

    // Now process a known value on both channels with same updateRms
    float testInput = 0.5f;
    sat.updateRms(testInput, testInput);
    float outL = sat.process(testInput);
    // Without re-calling updateRms, process the same value (same gain should be applied)
    float outR = sat.process(testInput);

    // Both outputs should be identical (same gain applied)
    REQUIRE(outL == outR);
}

TEST_CASE("FeedbackSaturator reset clears state", "[FeedbackSaturator]")
{
    FeedbackSaturator sat;
    sat.prepare(kSampleRate);

    // Build up RMS state
    for (int i = 0; i < 22050; ++i)
    {
        sat.updateRms(2.0f, 2.0f);
        sat.process(2.0f);
    }

    sat.reset();

    // After reset, small signal should pass near unity
    float input = 0.2f;
    sat.updateRms(input, input);
    float output = sat.process(input);
    REQUIRE(output > input * 0.95f);
    REQUIRE(output < input * 1.05f);
}
