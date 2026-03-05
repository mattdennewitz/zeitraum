#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/PluginProcessor.h"

TEST_CASE("Processor instantiates", "[processor]") {
    ZeitraumProcessor proc;
    REQUIRE(proc.getName() == "Zeitraum");
}

TEST_CASE("Silence in produces silence out", "[processor]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // Delay of silence is silence (ignoring character noise at default 25%)
    // With DryWetMixer at 50%, the dry path is silence and wet path is delay of silence
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE(std::abs(buffer.getSample(ch, i)) < 0.001f);
}

TEST_CASE("State round-trip on empty state", "[state]") {
    juce::MemoryBlock savedState;
    {
        ZeitraumProcessor proc;
        proc.getStateInformation(savedState);
    }
    REQUIRE(savedState.getSize() > 0);

    ZeitraumProcessor proc2;
    proc2.setStateInformation(savedState.getData(),
                               static_cast<int>(savedState.getSize()));

    // Verify default parameter values survive round-trip
    auto* baseDelay = proc2.apvts.getRawParameterValue("BASE_DELAY");
    REQUIRE(baseDelay != nullptr);
    REQUIRE_THAT(baseDelay->load(), Catch::Matchers::WithinAbs(80.0, 0.5));
}

TEST_CASE("Bus layout support", "[processor]") {
    ZeitraumProcessor proc;

    SECTION("accepts stereo in / stereo out") {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.isBusesLayoutSupported(layout));
    }
    SECTION("rejects mono input / stereo output") {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE_FALSE(proc.isBusesLayoutSupported(layout));
    }
}

TEST_CASE("All Phase 2 parameters exist", "[parameters]") {
    ZeitraumProcessor proc;

    // Global parameters
    REQUIRE(proc.apvts.getParameter("BASE_DELAY") != nullptr);
    REQUIRE(proc.apvts.getParameter("MULTIPLIER") != nullptr);
    REQUIRE(proc.apvts.getParameter("MIX") != nullptr);
    REQUIRE(proc.apvts.getParameter("CHARACTER") != nullptr);
    REQUIRE(proc.apvts.getParameter("QUANTIZE") != nullptr);

    // Per-tap parameters (8 taps x 2 params = 16)
    for (int i = 1; i <= 8; ++i)
    {
        auto id = juce::String(i);
        REQUIRE(proc.apvts.getParameter("TAP" + id + "_POS") != nullptr);
        REQUIRE(proc.apvts.getParameter("TAP" + id + "_LEVEL") != nullptr);
    }
}

TEST_CASE("Parameter defaults are correct", "[parameters]") {
    ZeitraumProcessor proc;

    auto load = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };

    REQUIRE_THAT(load("BASE_DELAY"), Catch::Matchers::WithinAbs(80.0, 0.5));
    REQUIRE_THAT(load("MULTIPLIER"), Catch::Matchers::WithinAbs(1.0, 0.05));
    REQUIRE_THAT(load("MIX"), Catch::Matchers::WithinAbs(50.0, 0.5));
    REQUIRE_THAT(load("CHARACTER"), Catch::Matchers::WithinAbs(25.0, 0.5));
    REQUIRE(load("QUANTIZE") < 0.5f); // false

    // Tap positions: equal spacing at i/8
    REQUIRE_THAT(load("TAP1_POS"), Catch::Matchers::WithinAbs(0.125, 0.005));
    REQUIRE_THAT(load("TAP8_POS"), Catch::Matchers::WithinAbs(1.0, 0.005));

    // All tap levels default to 1.0
    for (int i = 1; i <= 8; ++i)
        REQUIRE_THAT(load("TAP" + juce::String(i) + "_LEVEL"),
                     Catch::Matchers::WithinAbs(1.0, 0.005));
}

TEST_CASE("Delay produces output from impulse", "[processor][dsp]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Set MIX to 100% (wet only) to isolate delay output
    if (auto* param = proc.apvts.getParameter("MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(100.0f));

    // Create impulse in sample 0
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // First block: impulse just entered the delay line.
    // With default BASE_DELAY=80ms, TAP1_POS=0.125 -> 10ms -> ~441 samples
    // So the first tap output appears around sample 441 in this block or in a later block.

    // Process enough blocks to capture delayed output
    // TAP1 at 10ms = 441 samples, TAP8 at 80ms = 3528 samples
    // We need ~7 more blocks of 512 to cover all taps
    bool foundNonZero = false;
    for (int block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> nextBuffer(2, 512);
        nextBuffer.clear();
        proc.processBlock(nextBuffer, midi);

        for (int i = 0; i < 512; ++i)
        {
            if (std::abs(nextBuffer.getSample(0, i)) > 0.001f)
            {
                foundNonZero = true;
                break;
            }
        }
        if (foundNonZero) break;
    }

    REQUIRE(foundNonZero);
}

TEST_CASE("State round-trip preserves parameters", "[state]") {
    juce::MemoryBlock savedState;
    {
        ZeitraumProcessor proc;

        // Set non-default values
        if (auto* param = proc.apvts.getParameter("BASE_DELAY"))
            param->setValueNotifyingHost(param->convertTo0to1(120.0f));
        if (auto* param = proc.apvts.getParameter("TAP3_POS"))
            param->setValueNotifyingHost(param->convertTo0to1(0.5f));

        proc.getStateInformation(savedState);
    }

    REQUIRE(savedState.getSize() > 0);

    ZeitraumProcessor proc2;
    proc2.setStateInformation(savedState.getData(),
                               static_cast<int>(savedState.getSize()));

    auto load = [&](const juce::String& id) {
        return proc2.apvts.getRawParameterValue(id)->load();
    };

    REQUIRE_THAT(load("BASE_DELAY"), Catch::Matchers::WithinAbs(120.0, 1.0));
    REQUIRE_THAT(load("TAP3_POS"), Catch::Matchers::WithinAbs(0.5, 0.01));
}

TEST_CASE("Stereo channels both produce output", "[processor][dsp]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Set MIX to 100% (wet only)
    if (auto* param = proc.apvts.getParameter("MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(100.0f));

    // Feed impulse to both channels
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // Process enough blocks for delayed output to appear
    bool foundLeft = false, foundRight = false;
    for (int block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> nextBuffer(2, 512);
        nextBuffer.clear();
        proc.processBlock(nextBuffer, midi);

        for (int i = 0; i < 512; ++i)
        {
            if (std::abs(nextBuffer.getSample(0, i)) > 0.001f)
                foundLeft = true;
            if (std::abs(nextBuffer.getSample(1, i)) > 0.001f)
                foundRight = true;
        }
        if (foundLeft && foundRight) break;
    }

    REQUIRE(foundLeft);
    REQUIRE(foundRight);
}
