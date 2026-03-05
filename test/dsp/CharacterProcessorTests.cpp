#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/CharacterProcessor.h"
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    // Generate a sine wave buffer
    std::vector<float> generateSine(float freqHz, double sampleRate, int numSamples)
    {
        std::vector<float> buffer(static_cast<size_t>(numSamples));
        const float twoPi = 6.2831853f;
        for (int i = 0; i < numSamples; ++i)
            buffer[static_cast<size_t>(i)] = std::sin(twoPi * freqHz * static_cast<float>(i) / static_cast<float>(sampleRate));
        return buffer;
    }

    // Compute RMS of a buffer (optionally skip initial samples for filter settling)
    float computeRMS(const std::vector<float>& buffer, int skipSamples = 0)
    {
        double sum = 0.0;
        int count = 0;
        for (size_t i = static_cast<size_t>(skipSamples); i < buffer.size(); ++i)
        {
            sum += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i]);
            ++count;
        }
        return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
    }
}

TEST_CASE("CharacterProcessor clean passthrough", "[character]")
{
    CharacterProcessor cp;
    cp.prepare(44100.0, 2);

    const double sampleRate = 44100.0;
    const int numSamples = 4096;
    auto input = generateSine(1000.0f, sampleRate, numSamples);
    std::vector<float> output(static_cast<size_t>(numSamples));

    for (int i = 0; i < numSamples; ++i)
        output[static_cast<size_t>(i)] = cp.process(0, input[static_cast<size_t>(i)], 0.0f);

    // With characterAmount=0, output should equal input
    float inputRMS = computeRMS(input, 512);
    float outputRMS = computeRMS(output, 512);
    REQUIRE_THAT(outputRMS, WithinAbs(static_cast<double>(inputRMS), 0.001));
}

TEST_CASE("CharacterProcessor HF attenuation at full character", "[character]")
{
    CharacterProcessor cp;
    const double sampleRate = 44100.0;
    cp.prepare(sampleRate, 2);

    const int numSamples = 8192;
    auto lowInput = generateSine(100.0f, sampleRate, numSamples);
    auto highInput = generateSine(10000.0f, sampleRate, numSamples);

    std::vector<float> lowOutput(static_cast<size_t>(numSamples));
    std::vector<float> highOutput(static_cast<size_t>(numSamples));

    // Process low frequency (100Hz) through channel 0
    for (int i = 0; i < numSamples; ++i)
        lowOutput[static_cast<size_t>(i)] = cp.process(0, lowInput[static_cast<size_t>(i)], 1.0f);

    // Need a fresh processor for the high frequency test to avoid filter state contamination
    CharacterProcessor cp2;
    cp2.prepare(sampleRate, 2);

    // Process high frequency (10kHz) through channel 0
    for (int i = 0; i < numSamples; ++i)
        highOutput[static_cast<size_t>(i)] = cp2.process(0, highInput[static_cast<size_t>(i)], 1.0f);

    // Skip settling time, measure RMS
    float lowRMS = computeRMS(lowOutput, 2048);
    float highRMS = computeRMS(highOutput, 2048);

    // HF should be attenuated by at least 3dB relative to LF
    // 3dB means highRMS / lowRMS < 10^(-3/20) ~= 0.708
    float ratio = highRMS / lowRMS;
    REQUIRE(ratio < 0.708f);
}

TEST_CASE("CharacterProcessor noise floor presence", "[character]")
{
    CharacterProcessor cp;
    cp.prepare(44100.0, 2);

    // Process silence with character=1.0
    float noiseSum = 0.0f;
    const int numSamples = 4096;
    for (int i = 0; i < numSamples; ++i)
    {
        float out = cp.process(0, 0.0f, 1.0f);
        noiseSum += out * out;
    }
    float noiseRMS = std::sqrt(noiseSum / static_cast<float>(numSamples));
    // Noise should be present (above zero)
    REQUIRE(noiseRMS > 1e-6f);

    // Process silence with character=0.0
    CharacterProcessor cp2;
    cp2.prepare(44100.0, 2);
    float cleanSum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float out = cp2.process(0, 0.0f, 0.0f);
        cleanSum += out * out;
    }
    float cleanRMS = std::sqrt(cleanSum / static_cast<float>(numSamples));
    // Clean should have no noise
    REQUIRE(cleanRMS < 1e-6f);
}

TEST_CASE("CharacterProcessor dual-mono independence", "[character]")
{
    CharacterProcessor cp;
    cp.prepare(44100.0, 2);

    // Process different signals on channel 0 and 1
    float out0 = cp.process(0, 1.0f, 0.5f);
    float out1 = cp.process(1, -1.0f, 0.5f);

    // They should differ (different input, independent processing)
    REQUIRE(out0 != out1);

    // Signs should be preserved (lowpass of positive stays positive, negative stays negative)
    // After just one sample the filter won't fully converge but the sign should follow input
    REQUIRE(out0 > 0.0f);
    REQUIRE(out1 < 0.0f);
}
