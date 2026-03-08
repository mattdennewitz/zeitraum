# Phase 1: Project Scaffolding - Research

**Researched:** 2026-03-05
**Domain:** JUCE plugin scaffolding, CMake build system, AU validation, Catch2 testing
**Confidence:** HIGH

## Summary

Phase 1 creates a compiling pass-through JUCE plugin shell that builds as VST3 and AU, passes AU validation, and establishes the Makefile-driven workflow. The three-sisters project (`~/src/three-sisters/`) is the direct template -- same manufacturer, same build toolchain, proven patterns. The entire build system (CMakeLists.txt, Makefile, .gitignore, .gitmodules) can be adapted with minimal changes.

The primary risk is AU validation failure. The auval tool is strict about bus layouts, tail length reporting, and the `createPluginFilter` entry point. The three-sisters project has all of these correct, so following its patterns closely eliminates this risk. The secondary consideration is choosing unique plugin codes -- the CONTEXT.md specifies `TDPr` which is identical to three-sisters and will cause AU registration conflicts if both plugins are installed.

**Primary recommendation:** Clone the three-sisters build infrastructure with project-specific identity changes, implement a minimal pass-through processBlock, and verify AU validation before writing tests.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Plugin name in DAW: "Zeitraum"
- Manufacturer: "Die stille Erde"
- Manufacturer code (4-char): `DsEr`
- Plugin code (4-char): `TDPr`
- VST3 category: `Fx Delay`
- AU main type: `kAudioUnitType_Effect`
- IS_SYNTH: FALSE, no MIDI
- Follow three-sisters structure: `src/dsp/` for DSP, `src/ui/` for LookAndFeel and custom components
- Header-only DSP classes (matches three-sisters pattern)
- `src/PluginProcessor.h/cpp` and `src/PluginEditor.h/cpp` at top level of src/
- `test/` directory with `test/dsp/` subdirectory for DSP unit tests
- Include Catch2 v3.7.1 from phase 1 via FetchContent
- Test target shares plugin compile definitions via `$<TARGET_PROPERTY>` generator expression
- Initial test suite: processor instantiation, passthrough silence, state round-trip, bus layout
- CLAUDE.md adapted from three-sisters JUCEGUIDE.md
- JUCE as git submodule under `lib/JUCE` pinned to 8.0.12
- CMake + Ninja, wrapped by Makefile with targets: all, release, clean, test, validate, install, uninstall
- `COPY_PLUGIN_AFTER_BUILD=TRUE`
- `DONT_SET_USING_JUCE_NAMESPACE=1`
- Link: juce_audio_processors, juce_audio_utils, juce_gui_basics, juce_dsp

### Claude's Discretion
- Exact CMakeLists.txt structure (follow three-sisters pattern closely)
- Makefile check-tools implementation for auto-installing prerequisites
- Initial passthrough processBlock implementation
- GenericAudioProcessorEditor for initial editor (custom GUI is Phase 5)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INFR-01 | Plugin builds as VST3 and AU formats | CMakeLists.txt `FORMATS VST3 AU`, three-sisters template provides exact pattern |
| INFR-02 | Project uses CMake + Ninja with a Makefile automating all build tasks | Three-sisters Makefile is the direct template; all targets defined in CONTEXT.md |
| INFR-03 | AU validation passes via `auval` | Makefile `validate` target with AudioComponentRegistrar kill, isBusesLayoutSupported, proper processBlock clearing |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Audio plugin framework | Industry standard, proven in three-sisters |
| CMake | >= 3.22 | Build system | JUCE 8 requirement |
| Ninja | latest | Build backend | Faster than Make for CMake builds |
| Catch2 | v3.7.1 | Unit testing | Same as three-sisters, header-only via FetchContent |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce_audio_processors | 8.0.12 | AudioProcessor base, APVTS | Always -- core plugin infrastructure |
| juce_audio_utils | 8.0.12 | GenericAudioProcessorEditor | Phase 1 only (replaced by custom editor in Phase 5) |
| juce_gui_basics | 8.0.12 | Component base classes | Always -- required for editor |
| juce_dsp | 8.0.12 | DSP utilities (ScopedNoDenormals, future DelayLine) | Always -- needed from Phase 1 for ScopedNoDenormals |

**Installation:**
```bash
git submodule add https://github.com/juce-framework/JUCE.git lib/JUCE
cd lib/JUCE && git checkout 8.0.12 && cd ../..
```

## Architecture Patterns

### Project Structure
```
multi-tap-delay/
├── CMakeLists.txt
├── Makefile
├── CLAUDE.md
├── .gitignore
├── .gitmodules
├── lib/JUCE/                  # Git submodule pinned to 8.0.12
├── src/
│   ├── PluginProcessor.h      # AudioProcessor + APVTS
│   ├── PluginProcessor.cpp    # processBlock, state, createPluginFilter
│   ├── PluginEditor.h         # GenericAudioProcessorEditor wrapper
│   ├── PluginEditor.cpp
│   ├── dsp/                   # Header-only DSP classes (empty in Phase 1)
│   └── ui/                    # LookAndFeel, custom components (empty in Phase 1)
└── test/
    ├── PluginTests.cpp        # Processor integration tests
    └── dsp/                   # DSP unit tests (empty in Phase 1)
```

### Pattern 1: Pass-Through processBlock
**What:** Minimal processBlock that passes audio through unchanged with denormal protection and extra channel clearing.
**When to use:** Phase 1 scaffold -- proves plugin loads and processes audio.
**Example:**
```cpp
// Source: three-sisters JUCEGUIDE.md + three-sisters PluginProcessor pattern
void ZeitraumProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Clear any extra output channels beyond what we process
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Pass-through: input buffer is already the output buffer, nothing to do
}
```

### Pattern 2: APVTS Constructor Initialization
**What:** Initialize APVTS in the constructor initializer list with an empty parameter layout.
**When to use:** Phase 1 -- parameters will be added in Phase 2.
**Example:**
```cpp
// Source: three-sisters PluginProcessor pattern
ZeitraumProcessor::ZeitraumProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}
```

### Pattern 3: Stereo Bus Layout Support
**What:** Only accept stereo-in/stereo-out layout. Reject mono and other configurations.
**When to use:** Always -- this plugin is stereo-only.
**Example:**
```cpp
// Source: three-sisters PluginTests.cpp bus layout tests
bool ZeitraumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
```

### Pattern 4: State Persistence (Empty State)
**What:** Serialize/deserialize APVTS state with version number, even when no parameters exist yet.
**When to use:** From Phase 1 -- establishes forward-compatible state format.
**Example:**
```cpp
// Source: three-sisters JUCEGUIDE.md state persistence pattern
void ZeitraumProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr) { jassertfalse; return; }
    xml->setAttribute("pluginVersion", 1);
    copyXmlToBinary(*xml, destData);
}

void ZeitraumProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr) return;
    if (!xml->hasTagName(apvts.state.getType())) return;
    auto newState = juce::ValueTree::fromXml(*xml);
    if (!newState.isValid()) return;
    apvts.replaceState(newState);
}
```

### Anti-Patterns to Avoid
- **Forgetting `createPluginFilter`:** The build succeeds but the plugin won't load in any host. Must end PluginProcessor.cpp with `juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZeitraumProcessor(); }`
- **Using `JUCE_STANDALONE_APPLICATION` in test target:** Causes linker errors. The test target needs `JUCE_STANDALONE_APPLICATION=0` and `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0` in its compile definitions.
- **Not clearing extra output channels:** AU validation checks that channels beyond the input count contain silence. Omitting this causes auval failure.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Build system | Custom scripts | CMake + Ninja + Makefile | JUCE's `juce_add_plugin` handles format-specific packaging, code signing, plugin installation |
| Denormal protection | Manual FTZ/DAZ flags | `juce::ScopedNoDenormals` | Platform-specific (x86 vs ARM), JUCE handles both |
| Parameter system | Manual atomics | APVTS | Thread-safe, serialization, DAW automation, undo support |
| AU validation workflow | Manual auval invocation | Makefile `validate` target with `killall AudioComponentRegistrar` | AU cache is aggressive; forgetting the cache kill causes stale validation results |
| Plugin entry point | Manual factory | `createPluginFilter()` at end of PluginProcessor.cpp | JUCE linker expects this exact symbol |

## Common Pitfalls

### Pitfall 1: AU Cache Staleness
**What goes wrong:** Plugin changes don't appear in DAW or auval sees old version.
**Why it happens:** macOS caches AU component metadata in `AudioComponentRegistrar`.
**How to avoid:** Always kill the registrar before validation: `killall -9 AudioComponentRegistrar 2>/dev/null; true` followed by `sleep 1`.
**Warning signs:** auval passes but DAW shows old plugin behavior, or auval reports wrong channel counts.

### Pitfall 2: Duplicate Plugin Codes
**What goes wrong:** Two plugins with the same PLUGIN_CODE conflict in AU registration; one may not load.
**Why it happens:** The CONTEXT.md specifies `TDPr` as the plugin code, which is identical to the three-sisters project.
**How to avoid:** Use a unique 4-char PLUGIN_CODE for this project (e.g., `ZtRm` for Zeitraum). The PLUGIN_MANUFACTURER_CODE `DsEr` is correctly shared since it identifies the manufacturer, not the plugin.
**Warning signs:** auval shows wrong plugin name or wrong channel configuration.

### Pitfall 3: Missing JUCE Module Links
**What goes wrong:** Build errors like "Cannot find module juce_dsp".
**Why it happens:** Every `#include <juce_xxx/juce_xxx.h>` requires the corresponding `juce::juce_xxx` in `target_link_libraries`.
**How to avoid:** Always pair includes with link targets. Phase 1 needs: juce_audio_processors, juce_audio_utils, juce_gui_basics, juce_dsp.
**Warning signs:** Include errors or undefined symbol errors during linking.

### Pitfall 4: Test Target Compile Definitions
**What goes wrong:** Test binary crashes on startup or has linker errors.
**Why it happens:** Test target doesn't share the plugin's compile definitions (especially `DONT_SET_USING_JUCE_NAMESPACE`, `JUCE_VST3_CAN_REPLACE_VST2`, etc.).
**How to avoid:** Use `$<TARGET_PROPERTY:Zeitraum,COMPILE_DEFINITIONS>` in the test target's compile definitions.
**Warning signs:** "Ambiguous symbol" errors (missing namespace enforcement) or VST2-related linker errors.

### Pitfall 5: auval Code Format
**What goes wrong:** `auval -v aufx CODE MFRC` fails with "no matching AU found".
**Why it happens:** The codes passed to auval must match exactly what the AU component registers. The 4-char codes from CMake may be truncated or padded differently.
**How to avoid:** Check installed AU component with `auval -a` to find the exact subtype and manufacturer codes, then use those in the Makefile.
**Warning signs:** auval reports "no AudioUnits found that match" -- check codes with `auval -a | grep Zeitraum`.

## Code Examples

### CMakeLists.txt (adapted from three-sisters)
```cmake
# Source: ~/src/three-sisters/CMakeLists.txt (verified)
cmake_minimum_required(VERSION 3.22)
project(Zeitraum VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib/JUCE)

juce_add_plugin(Zeitraum
    COMPANY_NAME "Die stille Erde"
    BUNDLE_ID "com.diestilleerde.zeitraum"
    PLUGIN_MANUFACTURER_CODE DsEr
    PLUGIN_CODE ZtRm                    # UNIQUE -- not TDPr (three-sisters uses TDPr)
    FORMATS VST3 AU
    PRODUCT_NAME "Zeitraum"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay
)
```

### Makefile validate target (adapted from three-sisters)
```makefile
# Source: ~/src/three-sisters/Makefile (verified)
# Note: AU codes for auval need to match what the plugin registers
# Use `auval -a | grep -i zeitraum` after first build to confirm exact codes
PLUGIN_AU_CODE := ZtRm
PLUGIN_MFR_CODE := DsEr

validate: build-plugin
	@echo "== Validating AU plugin =="
	@killall -9 AudioComponentRegistrar 2>/dev/null; true
	@sleep 1
	@auval -v aufx $(PLUGIN_AU_CODE) $(PLUGIN_MFR_CODE)
```

### Catch2 Test Target (adapted from three-sisters)
```cmake
# Source: ~/src/three-sisters/CMakeLists.txt (verified)
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(ZeitraumTests test/PluginTests.cpp ${PLUGIN_SOURCES})
target_include_directories(ZeitraumTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ZeitraumTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
)
target_compile_definitions(ZeitraumTests PRIVATE
    $<TARGET_PROPERTY:Zeitraum,COMPILE_DEFINITIONS>
)

include(CTest)
add_test(NAME ZeitraumTests COMMAND ZeitraumTests)
```

### Phase 1 Test Suite (adapted from three-sisters PluginTests.cpp)
```cpp
// Source: ~/src/three-sisters/test/PluginTests.cpp (verified, Phase 1 subset)
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
    SECTION("rejects mono") {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE_FALSE(proc.isBusesLayoutSupported(layout));
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Projucer project files | CMake with `juce_add_plugin` | JUCE 6+ (2020) | No IDE dependency, scriptable builds |
| `using namespace juce` | `DONT_SET_USING_JUCE_NAMESPACE=1` | Best practice | Explicit `juce::` prefix avoids symbol conflicts |
| Manual parameter save/load | APVTS + XML serialization | JUCE 5+ | Thread-safe, automatic, DAW-compatible |
| Catch2 v2 `#define CATCH_CONFIG_MAIN` | Catch2 v3 `Catch2::Catch2WithMain` link target | Catch2 v3.0 (2022) | Separate library, faster compile |

## Open Questions

1. **Plugin Code Conflict**
   - What we know: CONTEXT.md specifies `TDPr` which is identical to three-sisters
   - What's unclear: Whether this was intentional (reuse) or accidental (copy-paste from discussion)
   - Recommendation: Use `ZtRm` instead. Flag to user if they insist on `TDPr`. Both plugins cannot be installed simultaneously with the same code.

2. **auval Code Format**
   - What we know: Three-sisters uses 3-char codes (`No3`, `DsE`) in Makefile but 4-char in CMake (`TDPr`, `DsEr`). The auval tool accepts 4-char codes.
   - What's unclear: Whether the 3-char codes in three-sisters Makefile are correct or if auval pads them
   - Recommendation: After first successful build, run `auval -a | grep -i zeitraum` to discover the exact registered codes, then update the Makefile accordingly. Use the full 4-char codes initially.

## Sources

### Primary (HIGH confidence)
- `~/src/three-sisters/CMakeLists.txt` - Direct template, verified working
- `~/src/three-sisters/Makefile` - Direct template, verified working
- `~/src/three-sisters/JUCEGUIDE.md` - Comprehensive JUCE development patterns
- `~/src/three-sisters/test/PluginTests.cpp` - Proven test patterns
- `~/src/three-sisters/src/PluginProcessor.h` - Proven processor structure
- JUCE 8.0.12 pinned in three-sisters submodule - verified version

### Secondary (MEDIUM confidence)
- auval code format behavior - based on three-sisters working example, but 3-char vs 4-char discrepancy noted

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - directly verified from working three-sisters project
- Architecture: HIGH - exact template exists and compiles/validates
- Pitfalls: HIGH - documented from three-sisters development experience (JUCEGUIDE.md)
- Plugin codes: MEDIUM - conflict risk identified, recommendation provided

**Research date:** 2026-03-05
**Valid until:** 2026-04-05 (stable -- JUCE 8.0.12 is pinned, Catch2 v3.7.1 is pinned)
