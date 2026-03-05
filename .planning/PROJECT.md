# Multi-Tap Delay

## What This Is

A JUCE-based audio plugin (VST3/AU/CLAP) that recreates the time-domain processing architecture of the Verbos Multi-Delay Processor and Buchla Model 288 Time Domain Processor. It provides 8 delay taps along a shared delay line with a full feedback routing matrix, per-tap level controls, preset mixes, and smooth delay-time modulation with doppler artifacts. Stereo in/out with optional cross-channel feedback.

## Core Value

The feedback routing matrix and per-tap control over a shared serial delay line — this is what creates the complex, evolving delay textures that distinguish these modules from standard multi-tap delays.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] 8-tap serial delay line with shared write head (stereo)
- [ ] Free-tap positioning with equal-spacing as default preset
- [ ] Per-tap level controls and individual tap outputs
- [ ] Preset mixes: odd taps, even taps, rising-level, falling-level
- [ ] Full NxN feedback routing matrix (any tap or preset mix back to input with independent gain)
- [ ] Cross-channel feedback routing for stereo
- [ ] Smooth delay time modulation with doppler/tape artifacts (no zipper noise)
- [ ] Delay range: ~10–150ms base, with multiplier dial extending to ~5s total
- [ ] Tap time quantization in coarse increments (10ms steps) with saveable presets
- [ ] All parameters DAW-automatable
- [ ] Character approximation: bandwidth limiting, mild noise, gentle HF roll-off in delay path
- [ ] Clean/modern DAW-style GUI
- [ ] VST3 and AU format support (CLAP deferred to v2)
- [ ] JUCE project with Makefile-automated build (following three-sisters pattern)

### Out of Scope

- Detailed DRAM/shift-register hardware emulation — character approximated via filtering/noise instead
- Mobile/iOS support — desktop DAW plugin only
- Standalone format — not needed for v1
- MIDI input/output — this is a pure audio effect

## Context

- Reference project: ~/src/three-sisters/ — successfully built JUCE plugin with CMake + Ninja + Makefile automation
- Inspired by: Verbos Multi-Delay Processor (equal-spaced taps, multiplier) and Buchla 288 Time Domain Processor (programmable tap presets, 10ms quantization)
- Both modules use 8 taps along a single delay line; the Verbos enforces equal spacing while the 288 allows arbitrary per-tap positioning
- Manufacturer: "Die stille Erde"
- The three-sisters project uses JUCE as a git submodule under lib/JUCE

## Constraints

- **Tech stack**: JUCE framework, C++17, CMake + Ninja build system
- **Platform**: macOS primary (AU validation), cross-platform later
- **Performance**: Must run real-time without glitches at standard buffer sizes (64–512 samples)
- **Delay architecture**: Single shared delay line per channel, not independent delay buffers per tap

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Free-tap mode with equal-spacing preset (not separate modes) | Simpler UX, equal spacing is just a preset within the free system | — Pending |
| Stereo in/out from v1 | Cross-feedback routing is core to the character | — Pending |
| Full NxN feedback matrix | Flexible feedback networks are what make these modules special | — Pending |
| CLAP deferred to v2 | Third-party extension, avoid blocking v1 on integration risk | — Pending |
| Clean/modern GUI style | Fits alongside stock DAW plugins rather than hardware skeuomorphism | — Pending |
| Follow three-sisters project structure | Proven JUCE + CMake + Makefile pattern | — Pending |

---
*Last updated: 2026-03-05 after requirements definition*
