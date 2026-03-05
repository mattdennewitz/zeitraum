#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/OnePoleSmooth.h"
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("OnePoleSmooth convergence", "[smoother]")
{
    OnePoleSmooth smoother;
    smoother.setSampleRate(44100.0);
    smoother.setTimeMs(10.0f);
    smoother.reset(0.0f);
    smoother.setTargetValue(1.0f);

    // After ~3x time constant, should be within 5% of target
    // time constant in samples = timeMs * 0.001 * sampleRate = 10 * 0.001 * 44100 = 441
    // 3x time constant = 1323 samples
    int samplesFor3TC = static_cast<int>(3.0 * 10.0 * 0.001 * 44100.0);
    float val = 0.0f;
    for (int i = 0; i < samplesFor3TC; ++i)
        val = smoother.getNextValue();

    REQUIRE_THAT(val, WithinAbs(1.0, 0.05));
}

TEST_CASE("OnePoleSmooth reset sets value immediately", "[smoother]")
{
    OnePoleSmooth smoother;
    smoother.setSampleRate(44100.0);
    smoother.setTimeMs(10.0f);
    smoother.reset(0.5f);

    float val = smoother.getNextValue();
    REQUIRE_THAT(val, WithinAbs(0.5, 1e-5));
}

TEST_CASE("OnePoleSmooth isSmoothing", "[smoother]")
{
    OnePoleSmooth smoother;
    smoother.setSampleRate(44100.0);
    smoother.setTimeMs(10.0f);

    // After reset, not smoothing
    smoother.reset(1.0f);
    REQUIRE_FALSE(smoother.isSmoothing());

    // After setting new target, smoothing
    smoother.setTargetValue(0.0f);
    REQUIRE(smoother.isSmoothing());

    // After many samples, converges and no longer smoothing
    for (int i = 0; i < 100000; ++i)
        smoother.getNextValue();
    REQUIRE_FALSE(smoother.isSmoothing());
}

TEST_CASE("OnePoleSmooth alpha formula matches CLAUDE.md", "[smoother]")
{
    OnePoleSmooth smoother;
    double sampleRate = 48000.0;
    float timeMs = 15.0f;
    smoother.setSampleRate(sampleRate);
    smoother.setTimeMs(timeMs);
    smoother.reset(0.0f);
    smoother.setTargetValue(1.0f);

    // Expected alpha: 1 - exp(-2*pi / (timeMs * 0.001 * sampleRate))
    float expectedAlpha = 1.0f - std::exp(-6.2831853f / (timeMs * 0.001f * static_cast<float>(sampleRate)));

    // After one sample: current = 0 + alpha * (1 - 0) = alpha
    float firstSample = smoother.getNextValue();
    REQUIRE_THAT(firstSample, WithinRel(expectedAlpha, 0.001));
}

TEST_CASE("OnePoleSmooth default values", "[smoother]")
{
    OnePoleSmooth smoother;
    // Default: timeMs=10.0, sampleRate=44100.0, current=0.0, target=0.0
    REQUIRE_FALSE(smoother.isSmoothing());
    float val = smoother.getNextValue();
    REQUIRE_THAT(val, WithinAbs(0.0, 1e-6));
}
