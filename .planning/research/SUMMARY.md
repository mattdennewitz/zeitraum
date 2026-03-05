# Project Research Summary

**Project:** Multi-Tap Delay
**Domain:** JUCE audio plugin (VST3/AU/CLAP) -- multi-tap delay with feedback matrix
**Researched:** 2026-03-05
**Confidence:** HIGH

## Executive Summary

This is a C++ audio plugin built with JUCE 8 that emulates the shared serial delay line architecture of the Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor. The key architectural insight is that this is NOT a collection of independent delay lines -- it is a single delay buffer per channel with 8 tap read positions, producing the inter-tap comb filtering and resonance patterns that define the product identity. The stack is well-proven: JUCE 8.0.12 via git submodule, CMake + Ninja build system, and the three-sisters reference project provides a verified pattern for project structure, build automation, parameter management, and AU validation.

The recommended approach is to build bottom-up: pure DSP primitives first (delay line, tap readers, feedback matrix), compose them into a per-channel engine, wire into the JUCE AudioProcessor shell, then build the GUI last. This ordering lets the DSP core be thoroughly unit-tested with Catch2 before any JUCE integration concerns arise. The feedback matrix (the primary differentiator) is architecturally a 24x2 matrix (8 taps + 4 preset mixes per channel, 2 destination channels), which is manageable as APVTS parameters with systematic naming.

The dominant risks are feedback instability (NxN routing makes gain loops trivially easy to create), delay time modulation artifacts (clicks from read pointer discontinuities), and AU validation failures (tail time, reset behavior, denormals). All three have well-known mitigations: tanh saturation in the feedback path, Lagrange3rd fractional-sample interpolation with parameter smoothing, and rigorous use of `auval` from the earliest functional build. CLAP support requires the external `clap-juce-extensions` library and should be integrated in the scaffolding phase rather than bolted on later.

## Key Findings

### Recommended Stack

The entire stack is anchored on JUCE 8.0.12 and the proven three-sisters project pattern. No external dependencies beyond JUCE, Catch2, and clap-juce-extensions.

**Core technologies:**
- **JUCE 8.0.12:** Plugin framework -- industry standard, provides audio processing, GUI, plugin format wrappers, and DSP primitives. Use as git submodule.
- **C++17:** Language standard -- JUCE 8 minimum requirement. C++20 adds no critical value here.
- **CMake 3.22+ / Ninja:** Build system -- canonical JUCE build approach via `juce_add_plugin()`. Makefile wraps for developer workflow.
- **juce::dsp::DelayLine (Lagrange3rd):** Core delay buffer -- supports multi-tap reads via `popSample(ch, delay, false)`. No custom buffer needed.
- **juce::AudioProcessorValueTreeState:** Parameter management -- thread-safe, handles DAW automation, XML state serialization, GUI binding.
- **Catch2 v3.7.1:** Unit testing -- proven in reference project, fetched via CMake FetchContent.
- **clap-juce-extensions:** CLAP format support -- not built into JUCE, requires FetchContent or submodule integration.

### Expected Features

**Must have (table stakes):**
- 8 delay taps with per-tap level control
- Wet/dry mix and feedback control
- Tempo sync with note divisions AND free-running (ms) mode
- HP/LP filtering in the delay/feedback path
- Stereo output with all parameters automatable
- Preset system (state save/restore)
- Smooth parameter changes (no clicks or zipper noise)
- Visual feedback of tap positions

**Should have (differentiators -- these ARE the product):**
- Shared serial delay line architecture (not independent buffers)
- Full NxN feedback routing matrix with visualization
- Cross-channel feedback routing for stereo spatial effects
- Preset tap patterns (odd/even/rising/falling) from Verbos
- Free tap positioning with equal-spacing as one preset
- Doppler/tape artifacts on delay time modulation
- Multiplier dial for proportional scaling of all tap times
- Analog character approximation (HF roll-off in feedback path)

**Defer (v2+):**
- Per-tap mute/solo (low complexity but not core identity)
- Sidechain ducking
- Advanced modulation system (DAW automation covers this)
- LV2 format (Linux-focused, trivial to add later)
- AAX format (requires Avid developer agreement)

### Architecture Approach

The architecture follows a thin AudioProcessor shell delegating to dedicated DSP components. Each stereo channel gets its own DelayEngine (owns a DelayLine + 8 TapReaders + CharacterFilter). The FeedbackMatrix lives outside the per-channel engines because it needs tap outputs from both channels for cross-channel routing. The matrix is 24 sources (8 taps + 4 preset mixes, per channel) into 2 destinations (L/R delay inputs) -- not a full NxN, which keeps parameter count manageable.

**Major components:**
1. **DelayEngine (x2)** -- Per-channel: writes to shared delay line, reads 8 taps with fractional interpolation, applies character filtering
2. **FeedbackMatrix** -- Computes feedback sums from all tap outputs across both channels, applies gain routing
3. **CrossChannelRouter** -- Manages L-to-R and R-to-L feedback exchange at the processor level
4. **PluginProcessor** -- JUCE lifecycle, APVTS parameter wiring, processBlock orchestration
5. **PluginEditor** -- GUI with tap position display, feedback matrix editor, preset mix controls

### Critical Pitfalls

1. **Feedback instability** -- NxN routing makes gain > 1.0 trivially easy. Apply tanh saturation in the feedback path and add NaN/inf detection with buffer reset. Do not try to constrain the UI; make the system robust to hot feedback.
2. **Delay time modulation clicks** -- Jumping the read pointer causes waveform discontinuities. Use Lagrange3rd fractional interpolation, smooth delay time with 50-100ms time constant, never snap the read pointer.
3. **AU validation failure** -- Return conservative tail time (10-30s), zero all buffers in reset(), use ScopedNoDenormals, run `auval` after every DSP change. Set up `make validate` in scaffolding.
4. **Real-time thread violations** -- No allocations in processBlock. Pre-allocate everything in prepareToPlay, use fixed-size std::array for the matrix, read parameters via atomic pointers only.
5. **CLAP not built into JUCE** -- Must integrate clap-juce-extensions from the start. Do not attempt to add "CLAP" to FORMATS list; it will not compile.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Project Scaffolding and Build System
**Rationale:** Everything depends on a working build. CLAP integration is easier to add now than retrofit. The three-sisters pattern provides a complete template.
**Delivers:** Compiling pass-through plugin in VST3, AU, and CLAP formats. Working Makefile with build/test/validate targets. AU validation passing on a no-op plugin.
**Addresses:** Plugin infrastructure (APVTS, format wrappers, state save/restore skeleton)
**Avoids:** CLAP integration failures (Pitfall 5), namespace inconsistency (Pitfall 11)

### Phase 2: Core DSP Engine
**Rationale:** The DSP engine is the foundation everything else builds on. Must be solid and well-tested before any UI or advanced features. This is where the most dangerous pitfalls live.
**Delivers:** Functional mono delay with 8 taps, basic feedback (global, not yet matrix), parameter smoothing, character filtering. Fully unit-tested with Catch2.
**Addresses:** Shared delay line, per-tap levels, wet/dry mix, free-running delay time, multiplier dial, doppler artifacts, analog character
**Avoids:** Feedback instability (Pitfall 1), delay time clicks (Pitfall 2), write/read ordering (Pitfall 6), buffer size errors (Pitfall 8), interpolation boundary errors (Pitfall 14), multiplier clicks (Pitfall 15)

### Phase 3: Feedback Matrix and Stereo
**Rationale:** The feedback matrix is the primary differentiator but depends on a working delay engine. Cross-channel routing adds the stereo dimension. This phase transforms a basic delay into the unique product.
**Delivers:** Full NxN feedback routing matrix, cross-channel feedback, preset tap mixes (odd/even/rising/falling), stereo processing.
**Addresses:** NxN feedback matrix, cross-channel routing, preset mixes, stereo output
**Avoids:** Gain staging issues (Pitfall 7), state save/restore of matrix (Pitfall 9), mono collapse (Pitfall 10)

### Phase 4: Tempo Sync and Parameter Polish
**Rationale:** Tempo sync depends on stable delay time infrastructure from Phase 2. Tap quantization and parameter naming conventions affect state persistence.
**Delivers:** Host BPM sync with note divisions, tap time quantization (10ms steps), free tap positioning with equal-spacing preset, finalized parameter IDs for all APVTS params.
**Addresses:** Tempo sync, tap quantization, free positioning, parameter serialization
**Avoids:** Quantization/sample-rate interaction (Pitfall 12)

### Phase 5: GUI Implementation
**Rationale:** GUI comes last because it depends on all parameters and DSP components being finalized. Building GUI before DSP is stable leads to rework.
**Delivers:** Clean/modern GUI with tap position display, interactive feedback matrix editor, preset mix controls, custom LookAndFeel.
**Addresses:** Visual feedback, feedback matrix visualization, preset mix controls, all UI features
**Avoids:** GUI repaint performance issues (Pitfall 13), thread safety between GUI and audio (Anti-Pattern 2)

### Phase 6: Validation, Testing, and Release Polish
**Rationale:** Final validation pass across all formats and sample rates. Ensures production quality.
**Delivers:** Passing AU validation at all sample rates, tested at 64-sample buffer sizes, preset library, final gain staging audit.
**Addresses:** AU validation (Pitfall 3), real-time safety (Pitfall 4), edge cases
**Avoids:** Shipping-blocking validation failures

### Phase Ordering Rationale

- Scaffolding first because all three plugin formats must build before writing DSP -- discovering build issues late is expensive.
- DSP before feedback matrix because the matrix operates on tap outputs that the engine produces. Cannot test the matrix without a working delay line.
- Feedback matrix before tempo sync because tempo sync is a parameter mapping concern, while the matrix is a core architectural component.
- GUI last because every parameter must exist before building controls for it. The APVTS parameter layout must be frozen before GUI binding.
- Validation is continuous (run `auval` after every phase) but gets a dedicated final phase for edge cases and release readiness.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 3 (Feedback Matrix):** The 24x2 matrix parameter naming scheme, gain staging normalization, and cross-channel mono collapse mitigation all need careful design. Research the spectral radius constraint for feedback stability if a more principled approach than tanh saturation is desired.
- **Phase 5 (GUI):** The feedback matrix editor UI is novel -- no standard JUCE pattern exists for an interactive NxN gain grid. May need custom component research.

Phases with standard patterns (skip deep research):
- **Phase 1 (Scaffolding):** Three-sisters provides a complete, verified template. Copy and adapt.
- **Phase 2 (Core DSP):** Delay lines, interpolation, and parameter smoothing are textbook DSP with JUCE-specific implementations already proven.
- **Phase 4 (Tempo Sync):** Standard JUCE host BPM integration via `getPlayHead()->getPosition()`.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | JUCE 8.0.12 verified from reference project source. DelayLine API confirmed from header docs. CMake pattern proven. |
| Features | MEDIUM | Hardware references (Verbos, Buchla 288) and competitive plugins based on training data knowledge. Feature set is well-reasoned but competitive landscape could not be verified against current product versions. |
| Architecture | HIGH | Pattern directly adapted from working three-sisters project. DSP architecture follows standard delay line principles. |
| Pitfalls | HIGH | Pitfalls derived from established JUCE development patterns, AU validation requirements, and standard DSP engineering. All mitigations are well-known techniques. |

**Overall confidence:** HIGH

### Gaps to Address

- **CLAP integration API stability:** The clap-juce-extensions project may have breaking changes. Pin to a specific commit and verify the CMake integration pattern against the current repo before starting Phase 1.
- **Feedback matrix parameter count:** 24x2 = 48 feedback gain parameters plus 8 tap levels, positions, and global controls could exceed 80 total APVTS parameters. Verify JUCE/DAW performance with this many automatable parameters. Some DAWs slow down with very large parameter counts.
- **Analog character tuning:** The FirstOrderTPTFilter for HF roll-off is the right tool, but the specific cutoff frequency and resonance values that sound "Verbos-like" or "Buchla-like" will need iterative tuning during Phase 2. No reference values available from research.
- **Custom delay buffer vs. JUCE DelayLine:** STACK.md recommends JUCE's DelayLine; ARCHITECTURE.md suggests a custom circular buffer may be more efficient for 8-tap reads. Decision should be made at the start of Phase 2 with a quick benchmark. Recommendation: start with JUCE DelayLine (less code, proven correctness) and only switch to custom if profiling shows a bottleneck.

## Sources

### Primary (HIGH confidence)
- Three-sisters reference project (`/Users/matt/src/three-sisters/`) -- CMakeLists.txt, Makefile, PluginProcessor, ParameterSmoother patterns
- JUCE 8.0.12 source code -- DelayLine API, SmoothedValue, TPT filters, APVTS, format support
- JUCE CMake API examples -- canonical `juce_add_plugin()` pattern

### Secondary (MEDIUM confidence)
- Verbos Multi-Delay Processor and Buchla 288 specifications -- hardware architecture and feature set
- Competitive plugin landscape (Valhalla Delay, EchoBoy, Timeless 3, UltraTap) -- feature expectations
- clap-juce-extensions project -- CMake integration pattern for CLAP format

### Tertiary (LOW confidence)
- Specific analog character filter values -- will need iterative tuning, no reference data available

---
*Research completed: 2026-03-05*
*Ready for roadmap: yes*
