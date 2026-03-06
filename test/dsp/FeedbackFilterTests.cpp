#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/FeedbackFilter.h"
#include <cmath>
#include <vector>

using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 44100.0;
static constexpr int kSettleBlocks = 4096;
static constexpr int kMeasureBlocks = 2048;

// Helper: generate sine wave samples
static std::vector<float> generateSine(float freqHz, double sampleRate, int numSamples)
{
    std::vector<float> out(static_cast<size_t>(numSamples));
    const double twoPi = 6.283185307179586;
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t>(i)] = static_cast<float>(std::sin(twoPi * freqHz * i / sampleRate));
    return out;
}

// Helper: compute RMS of a float range
static float computeRms(const float* data, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    return static_cast<float>(std::sqrt(sum / count));
}

// Helper: process sine through filter and return RMS of settled portion
static float measureFilteredRms(FeedbackFilter& filter, int channel, float freqHz,
                                 double sampleRate, int settleCount, int measureCount)
{
    auto sine = generateSine(freqHz, sampleRate, settleCount + measureCount);

    // Process settle portion (discard)
    for (int i = 0; i < settleCount; ++i)
        filter.process(channel, sine[static_cast<size_t>(i)]);

    // Process measure portion and collect output
    std::vector<float> output(static_cast<size_t>(measureCount));
    for (int i = 0; i < measureCount; ++i)
        output[static_cast<size_t>(i)] = filter.process(channel, sine[static_cast<size_t>(settleCount + i)]);

    return computeRms(output.data(), measureCount);
}

TEST_CASE("FeedbackFilter LP at 1kHz attenuates 10kHz by >6dB relative to 100Hz", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    filter.setLPFrequency(1000.0f);
    filter.setLPBypassed(false);

    float rms100Hz = measureFilteredRms(filter, 0, 100.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);
    filter.reset();
    float rms10kHz = measureFilteredRms(filter, 0, 10000.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);

    // 6dB = factor of 2. 10kHz should be at most half the amplitude of 100Hz.
    float ratio = rms10kHz / rms100Hz;
    REQUIRE(ratio < 0.5f);  // >6dB attenuation
}

TEST_CASE("FeedbackFilter HP at 1kHz attenuates 100Hz by >6dB relative to 10kHz", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    filter.setHPFrequency(1000.0f);
    filter.setHPBypassed(false);

    float rms10kHz = measureFilteredRms(filter, 0, 10000.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);
    filter.reset();
    float rms100Hz = measureFilteredRms(filter, 0, 100.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);

    float ratio = rms100Hz / rms10kHz;
    REQUIRE(ratio < 0.5f);  // >6dB attenuation
}

TEST_CASE("FeedbackFilter bypass: output equals input exactly", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    // Default: both bypassed

    auto sine = generateSine(440.0f, kSampleRate, 512);
    for (int i = 0; i < 512; ++i)
    {
        float out = filter.process(0, sine[static_cast<size_t>(i)]);
        REQUIRE(out == sine[static_cast<size_t>(i)]);
    }
}

TEST_CASE("FeedbackFilter LP bypass independent of HP", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    filter.setHPFrequency(1000.0f);
    filter.setHPBypassed(false);
    filter.setLPBypassed(true);  // LP stays bypassed

    // HP should attenuate 100Hz
    float rms100Hz = measureFilteredRms(filter, 0, 100.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);
    filter.reset();
    float rms10kHz = measureFilteredRms(filter, 0, 10000.0f, kSampleRate, kSettleBlocks, kMeasureBlocks);

    // 100Hz should be attenuated relative to 10kHz (HP is active)
    REQUIRE(rms100Hz / rms10kHz < 0.5f);
}

TEST_CASE("FeedbackFilter dual-mono: channels independent", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    filter.setLPFrequency(1000.0f);
    filter.setLPBypassed(false);

    // Process impulse on channel 0
    filter.process(0, 1.0f);
    float ch0_response = filter.process(0, 0.0f);

    // Channel 1 should have no state
    float ch1_out = filter.process(1, 0.0f);

    REQUIRE(ch0_response != 0.0f);  // ch0 has filter state
    REQUIRE(ch1_out == 0.0f);       // ch1 has no state
}

TEST_CASE("FeedbackFilter reset clears state", "[FeedbackFilter]")
{
    FeedbackFilter filter;
    filter.prepare(kSampleRate);
    filter.setLPFrequency(1000.0f);
    filter.setLPBypassed(false);

    // Build up state
    for (int i = 0; i < 100; ++i)
        filter.process(0, 1.0f);

    filter.reset();

    // After reset, processing zero should produce zero
    float out = filter.process(0, 0.0f);
    REQUIRE(out == 0.0f);
}
