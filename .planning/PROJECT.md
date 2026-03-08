# Multi-Tap Delay

## What This Is

A JUCE-based audio plugin (VST3/AU) that recreates the time-domain processing architecture of the Verbos Multi-Delay Processor and Buchla Model 288 Time Domain Processor. It provides 8 delay taps along a shared stereo delay line with a full feedback routing matrix, per-tap level controls, preset mixes, and smooth parameter modulation. Features a custom dark/teal GUI with draggable tap position bars and feedback matrix editor.

## Core Value

The feedback routing matrix and per-tap control over a shared serial delay line — this is what creates the complex, evolving delay textures that distinguish these modules from standard multi-tap delays.

## Requirements

### Validated

- ✓ 8-tap serial delay line with shared write head (stereo) — v1.0
- ✓ Free-tap positioning with equal-spacing as default preset — v1.0
- ✓ Per-tap level controls and individual tap outputs — v1.0
- ✓ Preset mixes: odd taps, even taps, rising-level, falling-level — v1.0
- ✓ Feedback routing matrix (any tap or preset mix back to input with independent gain) — v1.0
- ✓ Feedback saturation/limiting prevents runaway oscillation — v1.0
- ✓ Feedback HP+LP filters for tonal shaping — v1.0
- ✓ Interactive visual display of feedback routing matrix — v1.0
- ✓ Smooth delay time modulation (no zipper noise) — v1.0
- ✓ Delay range: ~10-150ms base, with multiplier dial extending to ~5s total — v1.0
- ✓ Tap time quantization in 10ms steps with saveable presets — v1.0
- ✓ All parameters DAW-automatable (38 params in organized groups) — v1.0
- ✓ Tempo sync with host BPM and 6 note divisions — v1.0
- ✓ Plugin state saves/restores with DAW session (XML v3) — v1.0
- ✓ Character approximation: HF roll-off, mild noise in delay path — v1.0
- ✓ Clean/modern DAW-style GUI with dark/teal theme — v1.0
- ✓ Visual display of tap positions with ms readouts and drag interaction — v1.0
- ✓ Per-tap level controls visible and adjustable in GUI — v1.0
- ✓ VST3 and AU format support — v1.0
- ✓ CMake + Ninja with Makefile automation — v1.0
- ✓ AU validation passes — v1.0
- ✓ Glitch-free at standard buffer sizes (64-512 samples) — v1.0
- ✓ Stereo operation (stereo in/out) — v1.0
- ✓ Wet/dry mix control — v1.0

### Active

- [ ] Doppler/tape artifacts when delay time is swept (pitch-shifting modulation character)
- [ ] Cross-channel feedback routing (L taps feed R input and vice versa)
- [ ] Per-tap mute/solo for quick auditioning
- [ ] CLAP format support via clap-juce-extensions

### Out of Scope

- Detailed DRAM/shift-register hardware emulation — character approximated via filtering/noise instead
- Mobile/iOS support — desktop DAW plugin only
- Standalone format — not needed
- MIDI input/output — this is a pure audio effect
- Independent delay buffers per tap — defeats the shared serial delay line architecture
- Granular/spectral processing — different product category
- Built-in reverb — dense multi-tap patterns already approach reverb-like textures
- Pattern/step sequencer — EchoBoy/UltraTap own this space
- Skeuomorphic hardware GUI — clean/modern DAW aesthetic decided during questioning
- Modulation matrix (LFO/envelope/XY) — FabFilter Timeless owns this; DAW automation covers complex modulation
- Sidechain ducking — not core; users can chain a separate ducker plugin
- Oversampling — delay effects don't typically alias
- Multi-band processing — scope creep; single full-bandwidth path with filtering

## Context

Shipped v1.0 with 2,408 LOC C++ source + 2,532 LOC tests. 1,810 test assertions.
Tech stack: JUCE 8.0.12, C++17, CMake + Ninja, Catch2 v3.7.1.
Reference: ~/src/three-sisters/ — successfully built JUCE plugin with same build pattern.
Inspired by: Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor.
Manufacturer: "Die stille Erde" (DsEr/ZtRm).

## Constraints

- **Tech stack**: JUCE framework, C++17, CMake + Ninja build system
- **Platform**: macOS primary (AU validation), cross-platform later
- **Performance**: Must run real-time without glitches at standard buffer sizes (64-512 samples)
- **Delay architecture**: Single shared delay line per channel, not independent delay buffers per tap

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Free-tap mode with equal-spacing preset (not separate modes) | Simpler UX, equal spacing is just a preset within the free system | ✓ Good — works naturally |
| Stereo in/out from v1 | Cross-feedback routing is core to the character | ✓ Good — dual-mono delay lines |
| Feedback routing matrix (12 sources, not full NxN) | 8 taps + 4 preset mixes covers all practical routing; NxN would be 64+ params | ✓ Good — sufficient flexibility |
| CLAP deferred to v2 | Third-party extension, avoid blocking v1 on integration risk | — Pending |
| Clean/modern GUI style | Fits alongside stock DAW plugins rather than hardware skeuomorphism | ✓ Good — dark/teal theme |
| Follow three-sisters project structure | Proven JUCE + CMake + Makefile pattern | ✓ Good — zero build issues |
| JUCE DelayLine over custom circular buffer | JUCE provides Lagrange3rd interpolation out of the box | ✓ Good — clean implementation |
| tanh soft clip for feedback saturation | Simple, bounded, preserves small-signal unity gain | ✓ Good — musical saturation |
| Apply-and-reset for OUTPUT_MIX presets | Avoids automation conflicts; preset is a trigger, not persistent state | ✓ Good — clean UX |
| 150ms base delay clamp with 33x multiplier | Total range ~5s; larger delays would require excessive memory | ⚠️ Revisit — tempo sync limited at slow BPMs |
| ParameterAttachment over SliderAttachment for custom components | Full gesture control (begin/set/end) needed for drag interaction | ✓ Good — proper automation recording |

---
*Last updated: 2026-03-08 after v1.0 milestone*
