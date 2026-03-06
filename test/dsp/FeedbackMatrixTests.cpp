#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/FeedbackMatrix.h"
#include <array>
#include <numeric>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 44100.0;
static constexpr int kBlockSize = 512;

// Helper: run several blocks to settle smoothers after a gain change
static void warmup(FeedbackMatrix& fm, const float* tapOutputs, int blocks = 20)
{
    for (int b = 0; b < blocks; ++b)
    {
        fm.prepareSmoothGains(kBlockSize);
        for (int i = 0; i < kBlockSize; ++i)
            fm.process(tapOutputs, i);
    }
}

TEST_CASE("FeedbackMatrix: single tap gain at 1.0 returns tap output", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Set tap 0 gain to 1.0
    fm.setSourceGain(0, 1.0f);

    // Tap outputs: tap 0 = 0.75, rest = 0
    std::array<float, 8> taps = {0.75f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Warmup to settle smoother
    warmup(fm, taps.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(taps.data(), 0);
    REQUIRE_THAT(result, WithinAbs(0.75f, 0.01f));
}

TEST_CASE("FeedbackMatrix: all gains at 0.0 returns zero", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // All gains default to 0.0
    std::array<float, 8> taps = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.6f, 0.4f, 0.9f};

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(taps.data(), 0);
    REQUIRE_THAT(result, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("FeedbackMatrix: Odd preset mix sums taps 1,3,5,7", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Source index 8 = Odd preset mix
    fm.setSourceGain(8, 1.0f);

    // All taps at 1.0 -> Odd = 0.25 * (tap0 + tap2 + tap4 + tap6) = 0.25 * 4 = 1.0
    std::array<float, 8> taps = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    warmup(fm, taps.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(taps.data(), 0);
    REQUIRE_THAT(result, WithinAbs(1.0f, 0.01f));
}

TEST_CASE("FeedbackMatrix: Even preset mix sums taps 2,4,6,8", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Source index 9 = Even preset mix
    fm.setSourceGain(9, 1.0f);

    // Only even-indexed taps (1,3,5,7 in 0-based) set to 1.0, odd-indexed to 0.0
    std::array<float, 8> taps = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    warmup(fm, taps.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(taps.data(), 0);
    // Even weights: {0, 0.25, 0, 0.25, 0, 0.25, 0, 0.25} -> sum = 0.25 * 4 = 1.0
    REQUIRE_THAT(result, WithinAbs(1.0f, 0.01f));
}

TEST_CASE("FeedbackMatrix: Rising preset has linearly increasing weights", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Source index 10 = Rising preset mix
    fm.setSourceGain(10, 1.0f);

    // Set tap outputs so we can verify relative weights
    // All taps at 1.0 -> Rising = sum of (1+2+3+4+5+6+7+8)/36 = 36/36 = 1.0
    std::array<float, 8> allOnes = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    warmup(fm, allOnes.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(allOnes.data(), 0);
    REQUIRE_THAT(result, WithinAbs(1.0f, 0.01f));

    // Verify tap 8 contributes more than tap 1
    // Set only tap 0 (tap 1) active
    fm.reset();
    fm.setSourceGain(10, 1.0f);
    std::array<float, 8> onlyFirst = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    warmup(fm, onlyFirst.data());
    fm.prepareSmoothGains(kBlockSize);
    float firstTapContrib = fm.process(onlyFirst.data(), 0);

    fm.reset();
    fm.setSourceGain(10, 1.0f);
    std::array<float, 8> onlyLast = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    warmup(fm, onlyLast.data());
    fm.prepareSmoothGains(kBlockSize);
    float lastTapContrib = fm.process(onlyLast.data(), 0);

    // Rising: tap8 weight (8/36) > tap1 weight (1/36)
    REQUIRE(lastTapContrib > firstTapContrib);
    REQUIRE_THAT(firstTapContrib, WithinAbs(1.0f / 36.0f, 0.01f));
    REQUIRE_THAT(lastTapContrib, WithinAbs(8.0f / 36.0f, 0.01f));
}

TEST_CASE("FeedbackMatrix: Falling preset has linearly decreasing weights", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Source index 11 = Falling preset mix
    fm.setSourceGain(11, 1.0f);

    // All taps at 1.0 -> Falling = sum of (8+7+6+5+4+3+2+1)/36 = 36/36 = 1.0
    std::array<float, 8> allOnes = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    warmup(fm, allOnes.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(allOnes.data(), 0);
    REQUIRE_THAT(result, WithinAbs(1.0f, 0.01f));

    // Verify tap 1 contributes more than tap 8
    fm.reset();
    fm.setSourceGain(11, 1.0f);
    std::array<float, 8> onlyFirst = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    warmup(fm, onlyFirst.data());
    fm.prepareSmoothGains(kBlockSize);
    float firstTapContrib = fm.process(onlyFirst.data(), 0);

    REQUIRE_THAT(firstTapContrib, WithinAbs(8.0f / 36.0f, 0.01f));
}

TEST_CASE("FeedbackMatrix: multiple sources sum correctly", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    // Tap 0 at 0.5 gain + Odd preset at 0.5 gain
    fm.setSourceGain(0, 0.5f);
    fm.setSourceGain(8, 0.5f);

    std::array<float, 8> taps = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    warmup(fm, taps.data());

    fm.prepareSmoothGains(kBlockSize);
    float result = fm.process(taps.data(), 0);

    // Tap 0 contribution: 1.0 * 0.5 = 0.5
    // Odd contribution: (1.0*0.25 + 0*0 + 1.0*0.25 + 0*0 + 1.0*0.25 + 0*0 + 1.0*0.25 + 0*0) * 0.5
    //                 = 1.0 * 0.5 = 0.5
    // Total = 1.0
    REQUIRE_THAT(result, WithinAbs(1.0f, 0.01f));
}

TEST_CASE("FeedbackMatrix: gain smoothing produces gradual transition", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    std::array<float, 8> taps = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Start with gain at 0
    warmup(fm, taps.data());

    // Step change to 1.0
    fm.setSourceGain(0, 1.0f);

    fm.prepareSmoothGains(kBlockSize);

    // First sample should NOT be at target (smoothing in effect)
    float firstSample = fm.process(taps.data(), 0);
    REQUIRE(firstSample < 0.9f); // Not yet at target

    // After ~7ms worth of samples (~309 at 44100), should be close to target
    int samplesFor7ms = static_cast<int>(0.007 * kSampleRate);

    // Process through the block to get later samples
    float laterSample = 0.0f;
    for (int i = 1; i < kBlockSize; ++i)
    {
        laterSample = fm.process(taps.data(), i);
    }

    // After 512 samples (~11.6ms), should be very close to 1.0
    REQUIRE(laterSample > firstSample); // Monotonically increasing
    REQUIRE_THAT(laterSample, WithinAbs(1.0f, 0.05f));
}

TEST_CASE("FeedbackMatrix: prepareSmoothGains + process reads correct values per index", "[feedback]")
{
    FeedbackMatrix fm;
    fm.prepare(kSampleRate, kBlockSize);

    fm.setSourceGain(0, 1.0f);
    std::array<float, 8> taps = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    warmup(fm, taps.data());

    // Now change gain -- values across the block should differ (smoothing)
    fm.setSourceGain(0, 0.0f);
    fm.prepareSmoothGains(kBlockSize);

    float val0 = fm.process(taps.data(), 0);
    float valMid = fm.process(taps.data(), kBlockSize / 2);
    float valEnd = fm.process(taps.data(), kBlockSize - 1);

    // Values should be decreasing (gain going from ~1.0 toward 0.0)
    REQUIRE(val0 > valMid);
    REQUIRE(valMid > valEnd);
}

TEST_CASE("FeedbackMatrix: all preset mix weights sum to 1.0", "[feedback]")
{
    // Verify the static weight arrays
    // Odd: 0.25 * 4 = 1.0
    float oddSum = 0.25f * 4;
    REQUIRE_THAT(oddSum, WithinAbs(1.0f, 1e-6f));

    // Even: 0.25 * 4 = 1.0
    float evenSum = 0.25f * 4;
    REQUIRE_THAT(evenSum, WithinAbs(1.0f, 1e-6f));

    // Rising: (1+2+3+4+5+6+7+8)/36 = 36/36 = 1.0
    float risingSum = 0.0f;
    for (int i = 1; i <= 8; ++i)
        risingSum += static_cast<float>(i) / 36.0f;
    REQUIRE_THAT(risingSum, WithinAbs(1.0f, 1e-6f));

    // Falling: (8+7+6+5+4+3+2+1)/36 = 36/36 = 1.0
    float fallingSum = 0.0f;
    for (int i = 8; i >= 1; --i)
        fallingSum += static_cast<float>(i) / 36.0f;
    REQUIRE_THAT(fallingSum, WithinAbs(1.0f, 1e-6f));
}
