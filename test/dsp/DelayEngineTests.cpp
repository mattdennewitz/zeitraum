#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/DelayEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>

using Catch::Matchers::WithinAbs;

namespace
{
    struct TapConfig
    {
        float positions[8];
        float levels[8];

        TapConfig()
        {
            for (int i = 0; i < 8; ++i)
            {
                positions[i] = static_cast<float>(i + 1) / 8.0f;
                levels[i] = 1.0f;
            }
        }
    };

    // Run warmup blocks to settle smoothers before the impulse test
    void warmup(DelayEngine& engine, int numSamples, float baseDelayMs, float multiplier,
                float characterAmount, bool quantize, const float* positions, const float* levels)
    {
        const int blockSize = 512;
        int processed = 0;
        while (processed < numSamples)
        {
            int thisBlock = std::min(blockSize, numSamples - processed);
            juce::AudioBuffer<float> buf(2, thisBlock);
            buf.clear();
            engine.process(buf, baseDelayMs, multiplier, characterAmount, quantize, positions, levels);
            processed += thisBlock;
        }
    }
}

TEST_CASE("DelayEngine basic delay line push-pop", "[delayengine]")
{
    // Minimal test: push an impulse, read it back after a known delay
    DelayEngine engine;
    const double sampleRate = 44100.0;
    engine.prepare(sampleRate, 1024);

    TapConfig config;
    // Use default positions (tap 0 at 0.125 = 1/8)
    // base delay 80ms, mult 1 -> tap 0: 0.125 * 80 = 10ms = 441 samples

    // Warmup: 44100 samples (1 second) to settle all smoothers completely
    warmup(engine, 44100, 80.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    // Now send impulse in a 1024-sample block
    juce::AudioBuffer<float> buffer(2, 1024);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, 80.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    // Check that SOME output is non-zero on channel 0
    float maxVal = 0.0f;
    int maxPos = -1;
    for (int i = 0; i < 1024; ++i)
    {
        float val = std::abs(buffer.getSample(0, i));
        if (val > maxVal)
        {
            maxVal = val;
            maxPos = i;
        }
    }

    INFO("maxVal=" << maxVal << " maxPos=" << maxPos);
    REQUIRE(maxVal > 0.01f);
}

TEST_CASE("DelayEngine impulse at correct position", "[delayengine]")
{
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 1024;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    // Single tap at position 1.0, all others muted
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;
    config.positions[0] = 1.0f;
    config.levels[0] = 1.0f;

    // Long warmup to fully settle smoothers
    warmup(engine, 44100, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    // Expected delay: 1.0 * 10ms * 1 = 441 samples
    const int expectedDelay = 441;

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    float peakSample = 0.0f;
    int peakPos = -1;
    for (int i = 1; i < blockSize; ++i)
    {
        float val = std::abs(buffer.getSample(0, i));
        if (val > peakSample)
        {
            peakSample = val;
            peakPos = i;
        }
    }

    INFO("peakSample=" << peakSample << " peakPos=" << peakPos);
    REQUIRE(peakPos >= 0);
    REQUIRE(std::abs(peakPos - expectedDelay) <= 3);
    REQUIRE(peakSample > 0.5f);
}

TEST_CASE("DelayEngine multi-tap produces 8 impulse copies", "[delayengine]")
{
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 4096;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;

    warmup(engine, 44100, 20.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, 20.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    const float threshold = 0.3f;
    int peakCount = 0;
    bool inPeak = false;
    for (int i = 50; i < 1000; ++i)
    {
        float val = std::abs(buffer.getSample(0, i));
        if (val > threshold && !inPeak)
        {
            peakCount++;
            inPeak = true;
        }
        else if (val < threshold * 0.5f)
        {
            inPeak = false;
        }
    }

    INFO("peakCount=" << peakCount);
    REQUIRE(peakCount == 8);
}

TEST_CASE("DelayEngine tap level at 0 produces silence for that tap", "[delayengine]")
{
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 2048;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;

    warmup(engine, 44100, 20.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, 20.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    float maxSample = 0.0f;
    for (int i = 1; i < blockSize; ++i)
        maxSample = std::max(maxSample, std::abs(buffer.getSample(0, i)));

    REQUIRE(maxSample < 1e-5f);
}

TEST_CASE("DelayEngine stereo independence", "[delayengine]")
{
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 1024;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;
    config.positions[0] = 1.0f;
    config.levels[0] = 1.0f;

    warmup(engine, 44100, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);

    engine.process(buffer, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    float leftMax = 0.0f;
    for (int i = 1; i < blockSize; ++i)
        leftMax = std::max(leftMax, std::abs(buffer.getSample(0, i)));

    float rightMax = 0.0f;
    for (int i = 1; i < blockSize; ++i)
        rightMax = std::max(rightMax, std::abs(buffer.getSample(1, i)));

    REQUIRE(leftMax > 0.5f);
    REQUIRE(rightMax < 1e-5f);
}

TEST_CASE("DelayEngine sweep produces no clicks", "[delayengine][glitch]")
{
    // Feed continuous sine wave, sweep base delay, check for sample-to-sample
    // discontinuities that would indicate clicks/glitches
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 64; // Small buffer size as reported problematic
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    // Use single tap to simplify analysis
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;
    config.positions[0] = 1.0f;
    config.levels[0] = 1.0f;

    // Warmup at initial delay
    warmup(engine, 44100, 80.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    // Generate sine input and collect output while sweeping delay
    const int totalBlocks = 200;  // ~290ms of audio
    std::vector<float> output;
    output.reserve(totalBlocks * blockSize);

    float baseDelay = 80.0f;
    for (int b = 0; b < totalBlocks; ++b)
    {
        // Sweep base delay from 80ms down to 30ms
        baseDelay = 80.0f - (50.0f * static_cast<float>(b) / static_cast<float>(totalBlocks));

        juce::AudioBuffer<float> buffer(2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            float t = static_cast<float>(b * blockSize + i) / static_cast<float>(sampleRate);
            float sample = std::sin(2.0f * 3.14159265f * 440.0f * t) * 0.5f;
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }

        engine.process(buffer, baseDelay, 1.0f, 0.0f, false,
                       config.positions, config.levels);

        for (int i = 0; i < blockSize; ++i)
            output.push_back(buffer.getSample(0, i));
    }

    // Detect clicks: look for sample-to-sample jumps that exceed a threshold.
    // A 440Hz sine at 0.5 amplitude has max sample-to-sample delta of about
    // 0.5 * 2*pi*440/44100 ≈ 0.031 per sample. Allow generous headroom for
    // pitch-shifted content during sweep (up to 4x), but flag anything >0.5
    // as a discontinuity (click).
    const float clickThreshold = 0.5f;
    int clickCount = 0;
    float maxDelta = 0.0f;

    // Skip first 1000 samples for smoother settling
    for (size_t i = 1001; i < output.size(); ++i)
    {
        float delta = std::abs(output[i] - output[i - 1]);
        if (delta > maxDelta) maxDelta = delta;
        if (delta > clickThreshold) clickCount++;
    }

    INFO("maxDelta=" << maxDelta << " clickCount=" << clickCount);
    REQUIRE(clickCount == 0);
}

TEST_CASE("DelayEngine tap position sweep produces no clicks", "[delayengine][glitch]")
{
    // Feed continuous sine wave, sweep tap position, check for discontinuities
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 64;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;
    config.levels[0] = 1.0f;
    config.positions[0] = 0.5f;

    warmup(engine, 44100, 80.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    const int totalBlocks = 200;
    std::vector<float> output;
    output.reserve(totalBlocks * blockSize);

    for (int b = 0; b < totalBlocks; ++b)
    {
        // Sweep tap position from 0.5 to 0.1
        config.positions[0] = 0.5f - (0.4f * static_cast<float>(b) / static_cast<float>(totalBlocks));

        juce::AudioBuffer<float> buffer(2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            float t = static_cast<float>(b * blockSize + i) / static_cast<float>(sampleRate);
            float sample = std::sin(2.0f * 3.14159265f * 440.0f * t) * 0.5f;
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }

        engine.process(buffer, 80.0f, 1.0f, 0.0f, false,
                       config.positions, config.levels);

        for (int i = 0; i < blockSize; ++i)
            output.push_back(buffer.getSample(0, i));
    }

    const float clickThreshold = 0.5f;
    int clickCount = 0;
    float maxDelta = 0.0f;

    for (size_t i = 1001; i < output.size(); ++i)
    {
        float delta = std::abs(output[i] - output[i - 1]);
        if (delta > maxDelta) maxDelta = delta;
        if (delta > clickThreshold) clickCount++;
    }

    INFO("maxDelta=" << maxDelta << " clickCount=" << clickCount);
    REQUIRE(clickCount == 0);
}

TEST_CASE("DelayEngine clean delay with characterAmount 0", "[delayengine]")
{
    DelayEngine engine;
    const double sampleRate = 44100.0;
    const int blockSize = 1024;
    engine.prepare(sampleRate, blockSize);

    TapConfig config;
    for (int i = 0; i < 8; ++i)
        config.levels[i] = 0.0f;
    config.positions[0] = 1.0f;
    config.levels[0] = 1.0f;

    warmup(engine, 44100, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, 10.0f, 1.0f, 0.0f, false, config.positions, config.levels);

    float peakVal = 0.0f;
    for (int i = 1; i < blockSize; ++i)
        peakVal = std::max(peakVal, std::abs(buffer.getSample(0, i)));

    REQUIRE(peakVal > 0.8f);
}
