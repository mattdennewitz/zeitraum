#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/PluginProcessor.h"

TEST_CASE("Tap positions are sorted ascending after randomization", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    for (int run = 0; run < 10; ++run)
    {
        proc.randomizeParameters();

        float prevPos = -1.0f;
        for (int i = 1; i <= 8; ++i)
        {
            auto* param = proc.apvts.getRawParameterValue(
                "TAP" + juce::String(i) + "_POS");
            float pos = param->load();
            REQUIRE(pos >= prevPos);
            prevPos = pos;
        }
    }
}

TEST_CASE("Feedback gain sum does not exceed 0.8 after randomization", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    for (int run = 0; run < 10; ++run)
    {
        proc.randomizeParameters();

        float sum = 0.0f;
        for (int i = 1; i <= 8; ++i)
        {
            auto* param = proc.apvts.getRawParameterValue(
                "FB_TAP" + juce::String(i));
            sum += param->load();  // actual value in [0, 100]
        }

        const juce::String mixNames[] = {"FB_ODD", "FB_EVEN", "FB_RISING", "FB_FALLING"};
        for (int i = 0; i < 4; ++i)
        {
            auto* param = proc.apvts.getRawParameterValue(mixNames[i]);
            sum += param->load();
        }

        // Sum of all 12 feedback sources in [0, 100] range should not exceed 80.0
        REQUIRE(sum <= 80.0f + 0.01f);  // small epsilon for float rounding
    }
}

TEST_CASE("MIX parameter is within [20, 90] after randomization", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    for (int run = 0; run < 10; ++run)
    {
        proc.randomizeParameters();

        auto* mixParam = proc.apvts.getRawParameterValue("MIX");
        float mix = mixParam->load();
        REQUIRE(mix >= 19.9f);  // small epsilon for float rounding
        REQUIRE(mix <= 90.1f);
    }
}

TEST_CASE("Mode parameters are unchanged after randomization", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Record pre-randomization values for excluded params
    auto* outputMix = proc.apvts.getRawParameterValue("OUTPUT_MIX");
    auto* tempoSync = proc.apvts.getRawParameterValue("TEMPO_SYNC");
    auto* quantize = proc.apvts.getRawParameterValue("QUANTIZE");
    auto* noteDiv = proc.apvts.getRawParameterValue("NOTE_DIV");

    float preOutputMix = outputMix->load();
    float preTempoSync = tempoSync->load();
    float preQuantize = quantize->load();
    float preNoteDiv = noteDiv->load();

    proc.randomizeParameters();

    REQUIRE(outputMix->load() == preOutputMix);
    REQUIRE(tempoSync->load() == preTempoSync);
    REQUIRE(quantize->load() == preQuantize);
    REQUIRE(noteDiv->load() == preNoteDiv);
}

TEST_CASE("FB_HP_FREQ < FB_LP_FREQ after randomization", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    for (int run = 0; run < 10; ++run)
    {
        proc.randomizeParameters();

        auto* hpFreq = proc.apvts.getRawParameterValue("FB_HP_FREQ");
        auto* lpFreq = proc.apvts.getRawParameterValue("FB_LP_FREQ");

        REQUIRE(hpFreq->load() < lpFreq->load());
    }
}

TEST_CASE("Two consecutive randomizations produce different values", "[randomizer]")
{
    ZeitraumProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    proc.randomizeParameters();

    // Capture first randomization
    std::vector<float> firstValues;
    for (int i = 1; i <= 8; ++i)
    {
        firstValues.push_back(
            proc.apvts.getRawParameterValue("TAP" + juce::String(i) + "_POS")->load());
        firstValues.push_back(
            proc.apvts.getRawParameterValue("TAP" + juce::String(i) + "_LEVEL")->load());
    }

    proc.randomizeParameters();

    // Capture second randomization
    std::vector<float> secondValues;
    for (int i = 1; i <= 8; ++i)
    {
        secondValues.push_back(
            proc.apvts.getRawParameterValue("TAP" + juce::String(i) + "_POS")->load());
        secondValues.push_back(
            proc.apvts.getRawParameterValue("TAP" + juce::String(i) + "_LEVEL")->load());
    }

    // At least one parameter must differ
    bool anyDifferent = false;
    for (size_t i = 0; i < firstValues.size(); ++i)
    {
        if (firstValues[i] != secondValues[i])
        {
            anyDifferent = true;
            break;
        }
    }
    REQUIRE(anyDifferent);
}
