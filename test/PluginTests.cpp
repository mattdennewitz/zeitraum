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

TEST_CASE("Tap preset save and recall", "[preset]") {
    ZeitraumProcessor proc;

    // Change TAP1_POS to 0.9
    if (auto* param = proc.apvts.getParameter("TAP1_POS"))
        param->setValueNotifyingHost(param->convertTo0to1(0.9f));

    proc.saveTapPreset("custom");

    // Reset TAP1_POS to default (0.125)
    if (auto* param = proc.apvts.getParameter("TAP1_POS"))
        param->setValueNotifyingHost(param->convertTo0to1(0.125f));

    // Verify it changed
    REQUIRE_THAT(proc.apvts.getRawParameterValue("TAP1_POS")->load(),
                 Catch::Matchers::WithinAbs(0.125, 0.01));

    // Recall the preset
    proc.recallTapPreset("custom");

    // Verify TAP1_POS is back to ~0.9
    REQUIRE_THAT(proc.apvts.getRawParameterValue("TAP1_POS")->load(),
                 Catch::Matchers::WithinAbs(0.9, 0.01));
}

TEST_CASE("Tap preset persists in state", "[preset][state]") {
    juce::MemoryBlock savedState;
    {
        ZeitraumProcessor proc;

        // Change some tap positions
        if (auto* param = proc.apvts.getParameter("TAP2_POS"))
            param->setValueNotifyingHost(param->convertTo0to1(0.7f));
        if (auto* param = proc.apvts.getParameter("TAP5_POS"))
            param->setValueNotifyingHost(param->convertTo0to1(0.33f));

        proc.saveTapPreset("my-preset");
        proc.getStateInformation(savedState);
    }

    REQUIRE(savedState.getSize() > 0);

    ZeitraumProcessor proc2;
    proc2.setStateInformation(savedState.getData(),
                               static_cast<int>(savedState.getSize()));

    // Verify preset name exists
    auto names = proc2.getTapPresetNames();
    REQUIRE(names.contains("my-preset"));

    // Reset tap positions to defaults, then recall
    if (auto* param = proc2.apvts.getParameter("TAP2_POS"))
        param->setValueNotifyingHost(param->convertTo0to1(0.25f));

    proc2.recallTapPreset("my-preset");

    REQUIRE_THAT(proc2.apvts.getRawParameterValue("TAP2_POS")->load(),
                 Catch::Matchers::WithinAbs(0.7, 0.01));
    REQUIRE_THAT(proc2.apvts.getRawParameterValue("TAP5_POS")->load(),
                 Catch::Matchers::WithinAbs(0.33, 0.01));
}

TEST_CASE("Recall nonexistent preset does nothing", "[preset]") {
    ZeitraumProcessor proc;

    // Record current positions
    float originalPos = proc.apvts.getRawParameterValue("TAP1_POS")->load();

    // Recall a nonexistent preset -- should not crash or change anything
    proc.recallTapPreset("does-not-exist");

    REQUIRE_THAT(proc.apvts.getRawParameterValue("TAP1_POS")->load(),
                 Catch::Matchers::WithinAbs(static_cast<double>(originalPos), 0.001));
}

TEST_CASE("Sweeping base delay produces no clicks at buffer size 64", "[processor][glitch]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 64);

    // Set MIX to 100% (wet only) and CHARACTER to 0% for clean signal
    if (auto* param = proc.apvts.getParameter("MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(100.0f));
    if (auto* param = proc.apvts.getParameter("CHARACTER"))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));

    // Use only tap 1 for cleaner analysis
    for (int i = 2; i <= 8; ++i)
        if (auto* param = proc.apvts.getParameter("TAP" + juce::String(i) + "_LEVEL"))
            param->setValueNotifyingHost(0.0f);

    juce::MidiBuffer midi;

    // Warmup for 1 second
    for (int b = 0; b < 690; ++b) {
        juce::AudioBuffer<float> buf(2, 64);
        for (int i = 0; i < 64; ++i) {
            float t = static_cast<float>(b * 64 + i) / 44100.0f;
            float sample = std::sin(2.0f * 3.14159265f * 440.0f * t) * 0.5f;
            buf.setSample(0, i, sample);
            buf.setSample(1, i, sample);
        }
        proc.processBlock(buf, midi);
    }

    // Sweep base delay from 80ms to 30ms over ~300ms (200 blocks of 64)
    std::vector<float> output;
    output.reserve(200 * 64);

    for (int b = 0; b < 200; ++b) {
        float baseDelay = 80.0f - (50.0f * static_cast<float>(b) / 200.0f);
        if (auto* param = proc.apvts.getParameter("BASE_DELAY"))
            param->setValueNotifyingHost(param->convertTo0to1(baseDelay));

        juce::AudioBuffer<float> buf(2, 64);
        for (int i = 0; i < 64; ++i) {
            float t = static_cast<float>((690 + b) * 64 + i) / 44100.0f;
            float sample = std::sin(2.0f * 3.14159265f * 440.0f * t) * 0.5f;
            buf.setSample(0, i, sample);
            buf.setSample(1, i, sample);
        }
        proc.processBlock(buf, midi);

        for (int i = 0; i < 64; ++i)
            output.push_back(buf.getSample(0, i));
    }

    // Check for discontinuities
    const float clickThreshold = 0.5f;
    int clickCount = 0;
    float maxDelta = 0.0f;

    for (size_t i = 1; i < output.size(); ++i) {
        float delta = std::abs(output[i] - output[i - 1]);
        if (delta > maxDelta) maxDelta = delta;
        if (delta > clickThreshold) clickCount++;
    }

    INFO("maxDelta=" << maxDelta << " clickCount=" << clickCount);
    REQUIRE(clickCount == 0);
}

// ============================================================
// Phase 3: Feedback parameter tests
// ============================================================

TEST_CASE("All Phase 3 feedback parameters exist", "[parameters][feedback]") {
    ZeitraumProcessor proc;

    // 8 individual tap feedback gains
    for (int i = 1; i <= 8; ++i)
        REQUIRE(proc.apvts.getParameter("FB_TAP" + juce::String(i)) != nullptr);

    // 4 preset mix feedback gains
    REQUIRE(proc.apvts.getParameter("FB_ODD") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_EVEN") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_RISING") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_FALLING") != nullptr);

    // Filter parameters
    REQUIRE(proc.apvts.getParameter("FB_HP_FREQ") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_LP_FREQ") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_HP_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("FB_LP_ON") != nullptr);
}

TEST_CASE("Feedback parameter defaults are correct", "[parameters][feedback]") {
    ZeitraumProcessor proc;

    auto load = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };

    // All gains default to 0%
    for (int i = 1; i <= 8; ++i)
        REQUIRE_THAT(load("FB_TAP" + juce::String(i)),
                     Catch::Matchers::WithinAbs(0.0, 0.5));

    REQUIRE_THAT(load("FB_ODD"), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(load("FB_EVEN"), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(load("FB_RISING"), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(load("FB_FALLING"), Catch::Matchers::WithinAbs(0.0, 0.5));

    // Filter defaults
    REQUIRE_THAT(load("FB_HP_FREQ"), Catch::Matchers::WithinAbs(20.0, 0.5));
    REQUIRE_THAT(load("FB_LP_FREQ"), Catch::Matchers::WithinAbs(20000.0, 1.0));

    // Filters bypassed by default
    REQUIRE(load("FB_HP_ON") < 0.5f);
    REQUIRE(load("FB_LP_ON") < 0.5f);
}

TEST_CASE("State round-trip preserves feedback parameters", "[state][feedback]") {
    juce::MemoryBlock savedState;
    {
        ZeitraumProcessor proc;

        // Set non-default feedback values
        if (auto* param = proc.apvts.getParameter("FB_TAP1"))
            param->setValueNotifyingHost(param->convertTo0to1(75.0f));
        if (auto* param = proc.apvts.getParameter("FB_ODD"))
            param->setValueNotifyingHost(param->convertTo0to1(50.0f));
        if (auto* param = proc.apvts.getParameter("FB_HP_FREQ"))
            param->setValueNotifyingHost(param->convertTo0to1(200.0f));
        if (auto* param = proc.apvts.getParameter("FB_LP_FREQ"))
            param->setValueNotifyingHost(param->convertTo0to1(5000.0f));
        if (auto* param = proc.apvts.getParameter("FB_HP_ON"))
            param->setValueNotifyingHost(1.0f);

        proc.getStateInformation(savedState);
    }

    REQUIRE(savedState.getSize() > 0);

    ZeitraumProcessor proc2;
    proc2.setStateInformation(savedState.getData(),
                               static_cast<int>(savedState.getSize()));

    auto load = [&](const juce::String& id) {
        return proc2.apvts.getRawParameterValue(id)->load();
    };

    REQUIRE_THAT(load("FB_TAP1"), Catch::Matchers::WithinAbs(75.0, 2.0));
    REQUIRE_THAT(load("FB_ODD"), Catch::Matchers::WithinAbs(50.0, 2.0));
    REQUIRE_THAT(load("FB_HP_FREQ"), Catch::Matchers::WithinAbs(200.0, 10.0));
    REQUIRE_THAT(load("FB_LP_FREQ"), Catch::Matchers::WithinAbs(5000.0, 100.0));
    REQUIRE(load("FB_HP_ON") > 0.5f);
}

TEST_CASE("Phase 2 state loads with feedback defaults", "[state][feedback]") {
    // Save state from a processor, then manually create a Phase 2 state
    // by saving with version 1 and no feedback params
    juce::MemoryBlock savedState;
    {
        ZeitraumProcessor proc;
        // Set a known non-default value for a Phase 2 param
        if (auto* param = proc.apvts.getParameter("BASE_DELAY"))
            param->setValueNotifyingHost(param->convertTo0to1(120.0f));
        proc.getStateInformation(savedState);
    }

    // Modify the XML to remove feedback params and set version to 1
    // (simulating a Phase 2 state)
    std::unique_ptr<juce::XmlElement> xml(
        juce::AudioProcessor::getXmlFromBinary(savedState.getData(),
                                                static_cast<int>(savedState.getSize())));
    REQUIRE(xml != nullptr);
    xml->setAttribute("pluginVersion", 1);

    // Remove all FB_ parameters from the XML
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        auto id = child->getStringAttribute("id");
        if (id.startsWith("FB_"))
            xml->removeChildElement(child, true);
        child = next;
    }

    // Re-serialize
    juce::MemoryBlock phase2State;
    juce::AudioProcessor::copyXmlToBinary(*xml, phase2State);

    // Load into new processor
    ZeitraumProcessor proc2;
    proc2.setStateInformation(phase2State.getData(),
                               static_cast<int>(phase2State.getSize()));

    auto load = [&](const juce::String& id) {
        return proc2.apvts.getRawParameterValue(id)->load();
    };

    // Phase 2 param should be preserved
    REQUIRE_THAT(load("BASE_DELAY"), Catch::Matchers::WithinAbs(120.0, 1.0));

    // Feedback params should be at defaults
    REQUIRE_THAT(load("FB_TAP1"), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(load("FB_ODD"), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(load("FB_HP_FREQ"), Catch::Matchers::WithinAbs(20.0, 0.5));
    REQUIRE_THAT(load("FB_LP_FREQ"), Catch::Matchers::WithinAbs(20000.0, 1.0));
    REQUIRE(load("FB_HP_ON") < 0.5f);
    REQUIRE(load("FB_LP_ON") < 0.5f);
}

// ============================================================
// Phase 3: Output mix preset tests
// ============================================================

TEST_CASE("OUTPUT_MIX parameter exists", "[parameters][output-mix]") {
    ZeitraumProcessor proc;

    auto* param = proc.apvts.getParameter("OUTPUT_MIX");
    REQUIRE(param != nullptr);

    // Should be a choice parameter with 5 options
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param);
    REQUIRE(choice != nullptr);
    REQUIRE(choice->choices.size() == 5);
    REQUIRE(choice->choices[0] == "Manual");
    REQUIRE(choice->choices[1] == "Odd");
    REQUIRE(choice->choices[2] == "Even");
    REQUIRE(choice->choices[3] == "Rising");
    REQUIRE(choice->choices[4] == "Falling");
}

TEST_CASE("Output mix Odd preset sets correct tap levels", "[processor][output-mix]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Set OUTPUT_MIX to Odd (index 1)
    if (auto* param = proc.apvts.getParameter("OUTPUT_MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(1.0f));

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    auto load = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };

    // Odd taps (1,3,5,7) should be 1.0, even taps (2,4,6,8) should be 0.0
    REQUIRE_THAT(load("TAP1_LEVEL"), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(load("TAP2_LEVEL"), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(load("TAP3_LEVEL"), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(load("TAP4_LEVEL"), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(load("TAP5_LEVEL"), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(load("TAP6_LEVEL"), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(load("TAP7_LEVEL"), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(load("TAP8_LEVEL"), Catch::Matchers::WithinAbs(0.0, 0.01));

    // OUTPUT_MIX should reset back to Manual (0)
    REQUIRE_THAT(load("OUTPUT_MIX"), Catch::Matchers::WithinAbs(0.0, 0.01));
}

TEST_CASE("Output mix Rising preset sets linear ramp", "[processor][output-mix]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Set OUTPUT_MIX to Rising (index 3)
    if (auto* param = proc.apvts.getParameter("OUTPUT_MIX"))
        param->setValueNotifyingHost(param->convertTo0to1(3.0f));

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    auto load = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };

    // Rising: 1/8, 2/8, 3/8, 4/8, 5/8, 6/8, 7/8, 1.0
    REQUIRE_THAT(load("TAP1_LEVEL"), Catch::Matchers::WithinAbs(1.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP2_LEVEL"), Catch::Matchers::WithinAbs(2.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP3_LEVEL"), Catch::Matchers::WithinAbs(3.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP4_LEVEL"), Catch::Matchers::WithinAbs(4.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP5_LEVEL"), Catch::Matchers::WithinAbs(5.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP6_LEVEL"), Catch::Matchers::WithinAbs(6.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP7_LEVEL"), Catch::Matchers::WithinAbs(7.0 / 8.0, 0.02));
    REQUIRE_THAT(load("TAP8_LEVEL"), Catch::Matchers::WithinAbs(1.0, 0.02));

    // TAP1 < TAP8
    REQUIRE(load("TAP1_LEVEL") < load("TAP8_LEVEL"));

    // OUTPUT_MIX should reset back to Manual (0)
    REQUIRE_THAT(load("OUTPUT_MIX"), Catch::Matchers::WithinAbs(0.0, 0.01));
}

TEST_CASE("Output mix Manual does not modify tap levels", "[processor][output-mix]") {
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Set custom tap levels
    if (auto* param = proc.apvts.getParameter("TAP1_LEVEL"))
        param->setValueNotifyingHost(param->convertTo0to1(0.42f));
    if (auto* param = proc.apvts.getParameter("TAP5_LEVEL"))
        param->setValueNotifyingHost(param->convertTo0to1(0.77f));

    // Ensure OUTPUT_MIX is Manual (0) -- should be default
    auto load = [&](const juce::String& id) {
        return proc.apvts.getRawParameterValue(id)->load();
    };
    REQUIRE_THAT(load("OUTPUT_MIX"), Catch::Matchers::WithinAbs(0.0, 0.01));

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // Tap levels should be unchanged
    REQUIRE_THAT(load("TAP1_LEVEL"), Catch::Matchers::WithinAbs(0.42, 0.02));
    REQUIRE_THAT(load("TAP5_LEVEL"), Catch::Matchers::WithinAbs(0.77, 0.02));
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
