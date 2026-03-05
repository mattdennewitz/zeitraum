# Technology Stack

**Project:** Multi-Tap Delay
**Researched:** 2026-03-05

## Recommended Stack

### Core Framework

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| JUCE | 8.0.12 | Audio plugin framework | Industry standard for cross-format audio plugins. Already proven in the three-sisters reference project. Provides audio processing, GUI, plugin format wrappers, and DSP primitives all in one framework. Use as git submodule under `lib/JUCE`. |
| C++ | C++17 | Language standard | JUCE 8 requires C++17 minimum. C++20 is possible but adds no critical features for this project and may cause issues with some JUCE internals. Stick with 17 for maximum compatibility with the proven three-sisters pattern. |
| CMake | >= 3.22 | Build system | JUCE 8's CMake API requires 3.22+. Use `juce_add_plugin()` for all format targets. This is the canonical JUCE build approach since JUCE 6. |
| Ninja | latest | Build backend | Fast incremental builds. Used in three-sisters Makefile pattern: `cmake -B build -G Ninja`. |

### Plugin Formats

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| VST3 | 3.8.0 (bundled) | DAW plugin format | Universal DAW support. VST3 SDK is bundled with JUCE 8 under MIT license -- no external SDK needed. |
| AU (AudioUnit) | System | macOS DAW format | Required for Logic Pro, GarageBand. Set `AU_MAIN_TYPE kAudioUnitType_Effect`. Validate with `auval`. |
| CLAP | via clap-juce-extensions | Modern plugin format | Growing adoption (Bitwig, Reaper, FL Studio). NOT natively supported by JUCE -- requires `free-audio/clap-juce-extensions` as a FetchContent dependency. See integration notes below. |

### DSP Components (all from juce::dsp)

| Component | Module | Purpose | Why |
|-----------|--------|---------|-----|
| `juce::dsp::DelayLine` | juce_dsp | Shared delay buffer | Built-in delay line with multi-tap support via `popSample(channel, delayInSamples, updateReadPointer=false)`. The `updateReadPointer=false` parameter enables reading multiple taps from a single delay line -- this is exactly the architecture needed. Use `Lagrange3rd` interpolation for smooth delay modulation without excessive filtering. |
| `juce::SmoothedValue` | juce_audio_basics | Parameter smoothing | Prevents zipper noise on parameter changes. Use `SmoothedValue<float, ValueSmoothingTypes::Multiplicative>` for gain parameters, `SmoothedValue<float, ValueSmoothingTypes::Linear>` for delay time. Reset smoothing ramp on `prepareToPlay`. |
| `juce::dsp::FirstOrderTPTFilter` | juce_dsp | HF roll-off in delay path | Topology-preserving transform (TPT) filter for the character approximation. Place a lowpass instance in the feedback path to simulate analog bandwidth limiting. Cheap, stable, musically appropriate. |
| `juce::dsp::StateVariableTPTFilter` | juce_dsp | Bandwidth limiting | 2nd-order SVF for more aggressive filtering if needed. Provides simultaneous lowpass/bandpass/highpass outputs. Use for input/output tone shaping. |
| `juce::dsp::ProcessorChain` | juce_dsp | DSP graph assembly | Template-based processor chain for composing filter stages. Useful for the per-tap processing chain. |
| `juce::AudioBuffer` | juce_audio_basics | Audio buffer management | Standard JUCE buffer type. Use for intermediate mixing of tap outputs and feedback routing. |

### Plugin Architecture (from juce_audio_processors)

| Component | Purpose | Why |
|-----------|---------|-----|
| `juce::AudioProcessorValueTreeState` (APVTS) | Parameter management | Connects parameters to GUI, handles DAW automation, provides thread-safe parameter access. Use `ParameterLayout` to declare all parameters. This is the standard JUCE pattern for automatable parameters. |
| `juce::AudioProcessor` | Plugin processor base class | Override `processBlock()`, `prepareToPlay()`, `releaseResources()`. Keep DSP in a separate engine class for testability. |
| `juce::AudioProcessorEditor` | Plugin GUI base class | Override for custom GUI. Use `juce::Component` hierarchy for the interface. |

### Testing

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Catch2 | v3.7.1 | Unit testing | Modern C++ test framework. Already used in three-sisters. Fetch via CMake `FetchContent`. Test DSP engine independently of plugin wrapper. |
| CTest | (CMake built-in) | Test runner | Standard CMake test integration. `make test` runs all Catch2 tests. |

### Build Automation

| Technology | Purpose | Why |
|------------|---------|-----|
| Makefile | Developer workflow | Wraps CMake/Ninja commands. Proven pattern from three-sisters: `make`, `make release`, `make clean`, `make validate`, `make test`. Handles submodule init, tool checking (auto-install via Homebrew), AU validation. |

## CLAP Integration Notes

JUCE does not natively support CLAP. The `clap-juce-extensions` project by the free-audio group adds CLAP as an additional format target. Integration pattern:

```cmake
# In CMakeLists.txt, after juce_add_plugin():
include(FetchContent)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main  # Pin to a specific commit/tag for reproducibility
)
FetchContent_MakeAvailable(clap-juce-extensions)

clap_juce_extensions_plugin(
    TARGET MultiTapDelay
    CLAP_ID "com.die-stille-erde.multi-tap-delay"
    CLAP_FEATURES audio-effect delay multi-effects
)
```

**Confidence: MEDIUM** -- This integration pattern is well-established in the JUCE community but I could not verify the exact current API against official docs due to tool limitations. The `clap-juce-extensions` repo should be checked for any breaking changes before integration. Consider deferring CLAP to a later phase if it blocks initial development.

## Delay Line Architecture Decision

**Use `juce::dsp::DelayLine` directly, do NOT write a custom delay buffer.**

The JUCE `DelayLine` class supports the exact multi-tap architecture needed:

```cpp
// Single shared delay line per channel
juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;

// Write once per sample
delayLine.pushSample(channel, inputSample);

// Read at 8 different tap positions (multi-tap)
for (int tap = 0; tap < 8; ++tap) {
    float tapOutput = delayLine.popSample(channel, tapDelaySamples[tap], false);  // false = don't advance read pointer
    // Mix tap output with per-tap gain...
}
// Advance read pointer once after all taps
delayLine.popSample(channel, 0.0f, true);  // true = advance read pointer
```

**Why Lagrange3rd interpolation:** Linear interpolation introduces audible low-pass filtering when modulating delay time. Lagrange3rd provides much better frequency response with minimal extra CPU cost (4 multiplies vs 1). Thiran is unsuitable because it is stateful and breaks when delay time is modulated rapidly -- which is a core feature (doppler/tape artifacts).

## Parameter Smoothing Strategy

| Parameter Type | Smoothing Method | Ramp Time | Why |
|----------------|-----------------|-----------|-----|
| Tap delay times | `SmoothedValue<float, Linear>` | 20-50ms | Smooth transitions create doppler pitch shift (desired tape effect). Do NOT snap -- gradual crossfade creates the analog character. |
| Tap levels / gains | `SmoothedValue<float, Multiplicative>` | 10-20ms | Multiplicative smoothing is perceptually linear for gain. Prevents clicks on gain changes. |
| Feedback matrix gains | `SmoothedValue<float, Multiplicative>` | 10-20ms | Same as tap levels. Critical to smooth these to avoid feedback instability during transitions. |
| Master mix (dry/wet) | `juce::dsp::DryWetMixer` | Built-in | JUCE's DryWetMixer handles crossfading with its own internal smoothing. |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Framework | JUCE 8 | iPlug2 | Less mature, smaller community, no proven reference project pattern |
| Framework | JUCE 8 | DPF (DISTRHO) | Good for simple plugins but lacks the DSP module library and GUI capabilities needed for an 8-tap delay with matrix routing |
| Build system | CMake + Ninja | Projucer | Projucer is legacy JUCE tooling. CMake is the modern approach since JUCE 6 and is what the reference project uses |
| Build system | CMake + Ninja | Meson, Bazel | Non-standard for JUCE projects. CMake is the only first-class build system JUCE supports |
| Delay interpolation | Lagrange3rd | Linear | Audible low-pass filtering during modulation |
| Delay interpolation | Lagrange3rd | Thiran | Stateful -- breaks with fast delay modulation, which is a core feature |
| Delay interpolation | Lagrange3rd | Custom (Hermite, sinc) | Unnecessary complexity. Lagrange3rd in JUCE is well-tested and performant |
| Testing | Catch2 v3 | GoogleTest | Either works, but Catch2 is already proven in the reference project |
| Parameter system | APVTS | Raw AudioProcessorParameter | APVTS provides attachment pattern for GUI binding, undo/redo support, XML state serialization for free |
| Custom delay buffer | juce::dsp::DelayLine | Hand-rolled circular buffer | DelayLine already handles interpolation, multi-tap reads, and buffer wrapping correctly. No reason to rewrite. |

## What NOT to Use

| Technology | Why Not |
|------------|---------|
| Projucer | Legacy IDE/project generator. CMake replaced it as the primary build approach in JUCE 6+. Creates non-standard project files that are harder to version control. |
| `juce::dsp::ProcessorDuplicator` | Tempting for stereo, but the cross-channel feedback routing requires explicit per-channel processing. Do not try to wrap the delay engine in ProcessorDuplicator. |
| JUCE's `AudioProcessorGraph` | Overkill for this plugin. The routing is a fixed 8-tap matrix, not a user-configurable graph. A direct DSP engine class is simpler and more performant. |
| VST2 | Deprecated by Steinberg. No SDK available. JUCE 8 still has code paths but actively discourages it. |
| AAX | Requires iLok/PACE signing infrastructure and Avid developer agreement. Not worth it for an initial release. |
| LV2 | Linux-focused format. macOS is the primary target. Can be added later trivially (just add `LV2` to FORMATS). |
| `juce_generate_juce_header` | Deprecated pattern. Include JUCE module headers directly (e.g., `#include <juce_dsp/juce_dsp.h>`). |

## Project Structure

Follow the three-sisters pattern:

```
multi-tap-delay/
  CMakeLists.txt          # Project root CMake
  Makefile                # Developer workflow automation
  lib/
    JUCE/                 # Git submodule, pinned to 8.0.12
  src/
    PluginProcessor.h     # AudioProcessor subclass
    PluginProcessor.cpp
    PluginEditor.h        # GUI editor
    PluginEditor.cpp
    dsp/
      DelayEngine.h       # Core DSP engine (testable independently)
      DelayEngine.cpp
      FeedbackMatrix.h    # NxN feedback routing
      FeedbackMatrix.cpp
      TapProcessor.h      # Per-tap processing (level, filter)
      TapProcessor.cpp
  test/
    PluginTests.cpp       # Catch2 test runner
    dsp/
      DelayEngineTests.cpp
      FeedbackMatrixTests.cpp
```

## Installation

```bash
# Clone with JUCE submodule
git submodule add https://github.com/juce-framework/JUCE.git lib/JUCE
cd lib/JUCE && git checkout 8.0.12 && cd ../..

# Build (Makefile handles cmake/ninja/submodule automatically)
make

# Release build
make release

# Run tests
make test

# Validate AU
make validate
```

No additional package installs needed -- the Makefile auto-installs cmake and ninja via Homebrew if missing. JUCE, Catch2, and clap-juce-extensions are all fetched as source dependencies.

## Confidence Assessment

| Component | Confidence | Notes |
|-----------|------------|-------|
| JUCE 8.0.12 | HIGH | Verified directly from the submodule in three-sisters. Version 8.0.12 confirmed in CMakeLists.txt and CHANGE_LIST.md. |
| CMake + Ninja pattern | HIGH | Directly verified from working three-sisters Makefile and CMakeLists.txt. |
| juce::dsp::DelayLine | HIGH | Read the full source code. Multi-tap via `popSample(ch, delay, false)` confirmed in the header documentation. |
| Lagrange3rd interpolation | HIGH | Read the interpolation implementation. 4-point interpolation confirmed. |
| SmoothedValue | HIGH | Verified in juce_audio_basics module source. |
| APVTS pattern | HIGH | Standard JUCE pattern, used universally in JUCE plugin development. |
| Catch2 v3.7.1 | HIGH | Verified from three-sisters CMakeLists.txt FetchContent declaration. |
| CLAP via clap-juce-extensions | MEDIUM | Known community project but could not verify current API/version. Pin to a specific commit. |
| FirstOrderTPTFilter for character | MEDIUM | Standard approach for analog-style filtering, but the specific character (Verbos/Buchla emulation) will need tuning during development. |

## Sources

- Three-sisters reference project: `/Users/matt/src/three-sisters/CMakeLists.txt` (verified CMake pattern, JUCE version, Catch2 integration)
- Three-sisters Makefile: `/Users/matt/src/three-sisters/Makefile` (verified build automation pattern)
- JUCE 8.0.12 source: `/Users/matt/src/three-sisters/lib/JUCE/` (verified DelayLine API, SmoothedValue, filter classes, supported formats)
- JUCE CHANGE_LIST.md: Confirmed version 8.0.12 with VST3 SDK 3.8.0 bundled
- JUCE CMake API: `/Users/matt/src/three-sisters/lib/JUCE/examples/CMake/AudioPlugin/CMakeLists.txt` (canonical plugin CMake pattern)
- JUCE format support: `/Users/matt/src/three-sisters/lib/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake` (confirmed AU, AUv3, AAX, LV2, VST, VST3, Standalone, Unity -- no CLAP)
