#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/TapReader.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("TapReader delay calculation", "[tapreader]")
{
    TapReader tap;
    tap.prepare(44100.0);

    SECTION("position 0.5 with base 80ms mult 1 gives 1764 samples")
    {
        tap.setPosition(0.5f);
        // Pump smoother to settle
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(80.0f, 1.0f, 44100.0, false);

        float delay = tap.getDelaySamples(80.0f, 1.0f, 44100.0, false);
        // 0.5 * 80 * 0.001 * 44100 = 1764
        REQUIRE_THAT(delay, WithinAbs(1764.0f, 1.0f));
    }

    SECTION("position 1.0 with base 100ms mult 2 gives 8820 samples")
    {
        tap.setPosition(1.0f);
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(100.0f, 2.0f, 44100.0, false);

        float delay = tap.getDelaySamples(100.0f, 2.0f, 44100.0, false);
        // 1.0 * 100 * 2 * 0.001 * 44100 = 8820
        REQUIRE_THAT(delay, WithinAbs(8820.0f, 1.0f));
    }

    SECTION("minimum 1 sample clamp")
    {
        tap.setPosition(0.0f);
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(10.0f, 1.0f, 44100.0, false);

        float delay = tap.getDelaySamples(10.0f, 1.0f, 44100.0, false);
        REQUIRE(delay >= 1.0f);
    }
}

TEST_CASE("TapReader quantization", "[tapreader]")
{
    TapReader tap;
    tap.prepare(44100.0);

    SECTION("snaps to 10ms grid: position 0.37 base 100ms mult 1 -> 40ms")
    {
        tap.setPosition(0.37f);
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(100.0f, 1.0f, 44100.0, true);

        float delay = tap.getDelaySamples(100.0f, 1.0f, 44100.0, true);
        // 0.37 * 100 = 37ms -> snaps to 40ms -> 40 * 0.001 * 44100 = 1764
        REQUIRE_THAT(delay, WithinAbs(1764.0f, 1.0f));
    }

    SECTION("quantize minimum 10ms: very small position")
    {
        tap.setPosition(0.01f);
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(10.0f, 1.0f, 44100.0, true);

        float delay = tap.getDelaySamples(10.0f, 1.0f, 44100.0, true);
        // 0.01 * 10 = 0.1ms -> would round to 0ms -> enforced min 10ms -> 10 * 0.001 * 44100 = 441
        REQUIRE_THAT(delay, WithinAbs(441.0f, 1.0f));
    }

    SECTION("quantize rounds 32ms to 30ms")
    {
        tap.setPosition(0.32f);
        for (int i = 0; i < 44100; ++i)
            tap.getDelaySamples(100.0f, 1.0f, 44100.0, true);

        float delay = tap.getDelaySamples(100.0f, 1.0f, 44100.0, true);
        // 0.32 * 100 = 32ms -> round(3.2)*10 = 30ms -> 30 * 0.001 * 44100 = 1323
        REQUIRE_THAT(delay, WithinAbs(1323.0f, 1.0f));
    }
}

TEST_CASE("TapReader default positions", "[tapreader]")
{
    SECTION("all 8 default positions are evenly spaced")
    {
        for (int i = 0; i < 8; ++i)
        {
            float expected = static_cast<float>(i + 1) / 8.0f;
            REQUIRE_THAT(TapReader::defaultPosition(i), WithinAbs(expected, 1e-6f));
        }
    }

    SECTION("tap 0 is 0.125, tap 7 is 1.0")
    {
        REQUIRE_THAT(TapReader::defaultPosition(0), WithinAbs(0.125f, 1e-6f));
        REQUIRE_THAT(TapReader::defaultPosition(7), WithinAbs(1.0f, 1e-6f));
    }
}

TEST_CASE("TapReader overlap", "[tapreader]")
{
    TapReader tap1, tap2;
    tap1.prepare(44100.0);
    tap2.prepare(44100.0);

    tap1.setPosition(0.5f);
    tap2.setPosition(0.5f);

    // Settle smoothers
    for (int i = 0; i < 44100; ++i)
    {
        tap1.getDelaySamples(80.0f, 1.0f, 44100.0, false);
        tap2.getDelaySamples(80.0f, 1.0f, 44100.0, false);
    }

    float delay1 = tap1.getDelaySamples(80.0f, 1.0f, 44100.0, false);
    float delay2 = tap2.getDelaySamples(80.0f, 1.0f, 44100.0, false);

    REQUIRE_THAT(delay1, WithinAbs(delay2, 0.001f));
}
