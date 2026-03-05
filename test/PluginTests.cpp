#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/PluginProcessor.h"

TEST_CASE("Processor instantiates", "[processor]") {
    ZeitraumProcessor proc;
    REQUIRE(proc.getName() == "Zeitraum");
}

TEST_CASE("Passthrough silence", "[processor]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // Output should be exactly zero (no garbage, no denormals)
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE(buffer.getSample(ch, i) == 0.0f);
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
    // Should not crash; no parameters to verify yet
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
