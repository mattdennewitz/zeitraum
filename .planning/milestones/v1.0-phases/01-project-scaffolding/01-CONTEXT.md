# Phase 1: Project Scaffolding - Context

**Gathered:** 2026-03-05
**Status:** Ready for planning

<domain>
## Phase Boundary

A compiling pass-through plugin shell that builds as VST3 and AU, passes AU validation, and provides the Makefile-driven workflow for all subsequent development. Includes test infrastructure and CLAUDE.md.

</domain>

<decisions>
## Implementation Decisions

### Plugin Identity
- Plugin name in DAW: "Zeitraum"
- Manufacturer: "Die stille Erde"
- Manufacturer code (4-char): `DsEr`
- Plugin code (4-char): `TDPr`
- VST3 category: `Fx Delay`
- AU main type: `kAudioUnitType_Effect`
- IS_SYNTH: FALSE, no MIDI

### Source Layout
- Follow three-sisters structure: `src/dsp/` for DSP, `src/ui/` for LookAndFeel and custom components
- Header-only DSP classes (matches three-sisters pattern, simpler build, easier testing)
- `src/PluginProcessor.h/cpp` and `src/PluginEditor.h/cpp` at top level of src/
- `test/` directory with `test/dsp/` subdirectory for DSP unit tests

### Test Infrastructure
- Include Catch2 v3.7.1 from phase 1 via FetchContent (same as three-sisters)
- Test target shares plugin compile definitions via `$<TARGET_PROPERTY>` generator expression
- Initial test suite verifies:
  - Processor instantiation (creates without crashing, prepareToPlay succeeds)
  - Passthrough silence (silent input → silent output, no garbage, no denormals)
  - State round-trip (getStateInformation / setStateInformation on empty state)
  - Bus layout (stereo in/out layout is supported)

### CLAUDE.md
- Adapt and merge relevant parts from three-sisters JUCEGUIDE.md (skip oversampling, radio buttons, etc.)
- Include: build commands reference, code conventions (juce:: prefix, header-only DSP, audio thread rules)
- Include: project-specific notes (plugin codes, manufacturer, delay architecture decisions)
- Written as part of phase 1 scaffold

### Build System
- JUCE as git submodule under `lib/JUCE` (pin to 8.0.12, matching three-sisters)
- CMake + Ninja, wrapped by Makefile with targets: all, release, clean, test, validate, install, uninstall
- `COPY_PLUGIN_AFTER_BUILD=TRUE` for auto-install
- `DONT_SET_USING_JUCE_NAMESPACE=1` (forces explicit juce:: prefix)
- Link: juce_audio_processors, juce_audio_utils, juce_gui_basics, juce_dsp

### Claude's Discretion
- Exact CMakeLists.txt structure (follow three-sisters pattern closely)
- Makefile check-tools implementation for auto-installing prerequisites
- Initial passthrough processBlock implementation
- GenericAudioProcessorEditor for initial editor (custom GUI is Phase 5)

</decisions>

<specifics>
## Specific Ideas

- Reference project: ~/src/three-sisters/ — proven JUCE + CMake + Makefile pattern to follow closely
- JUCEGUIDE.md from three-sisters contains comprehensive build/development patterns — adapt into CLAUDE.md
- Plugin should support stereo buses from the start (isBusesLayoutSupported must accept stereo in/out)
- Use `juce::ScopedNoDenormals` in processBlock from day 1

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- three-sisters CMakeLists.txt: Direct template for plugin CMake config
- three-sisters Makefile: Direct template for build automation (check-tools, validate, uninstall)
- three-sisters JUCEGUIDE.md: Comprehensive reference for JUCE development patterns

### Established Patterns
- JUCE submodule at lib/JUCE with specific version pin
- APVTS for all parameters with getRawParameterValue caching
- Header-only DSP in src/dsp/
- Catch2 FetchContent with shared compile definitions

### Integration Points
- No existing code — this phase creates the foundation everything builds on

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-project-scaffolding*
*Context gathered: 2026-03-05*
