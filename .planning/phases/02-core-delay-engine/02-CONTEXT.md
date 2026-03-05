# Phase 2: Core Delay Engine - Context

**Gathered:** 2026-03-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Functional stereo 8-tap delay with independent tap positioning, per-tap levels, wet/dry mix, delay time multiplier, tap quantization, and analog character. This is the sonic foundation -- no feedback routing (Phase 3), no tempo sync (Phase 4), no custom GUI (Phase 5).

</domain>

<decisions>
## Implementation Decisions

### Analog Character
- Subtle vintage bucket-brigade style: mild HF loss + slight noise floor, cleaner than tape but not pristine
- Single "Character" knob that scales coloring from clean to full vintage (one parameter, not separate HF/saturation controls)
- Coloring applies per-repeat (cumulative) -- each pass through the delay line adds more color, so echoes progressively darken
- Subtle noise floor included, scaled by the Character knob -- only audible in quiet passages or with high feedback

### Tap Positioning Model
- Ratio-based: each tap has a position from 0.0-1.0 along the delay line
- Actual delay time = position * (base delay * multiplier), so taps scale proportionally when base time changes
- Quantization (10ms steps) snaps the actual delay time, not just the display -- what you hear matches what you see
- Tap overlap is allowed -- taps can share the same position, producing comb filtering/doubling effects intentionally
- Default preset: even distribution at 1/8, 2/8, 3/8... 8/8 of the delay line

### Claude's Discretion
- Parameter ranges and curves (base delay 10-150ms range, multiplier range, level curves linear vs dB)
- Smoothing time constants for parameter changes (within the constraint of no clicks/zipper noise)
- DSP implementation details (interpolation method for delay line, filter topology for character)
- Tap preset save/recall mechanism

</decisions>

<specifics>
## Specific Ideas

- Character inspired by analog BBD delays (bucket-brigade) rather than tape echo or digital
- Cumulative darkening on repeats is essential -- this is what gives the delay its organic, evolving quality
- Comb filtering from overlapping taps is a feature, not a bug -- users should be able to explore those textures

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- ZeitraumProcessor: pass-through shell with APVTS, stereo bus config, state save/restore with XML versioning
- Empty `createParameterLayout()` ready for Phase 2 parameters
- `juce_dsp` module already linked (provides DelayLine, IIR filters, etc.)

### Established Patterns
- `DONT_SET_USING_JUCE_NAMESPACE=1` -- always use `juce::` prefix
- Parameter caching via `getRawParameterValue` + `.load()` in processBlock
- Header-only DSP classes in `src/dsp/`, JUCE-free when possible
- State persistence: XML with `pluginVersion` attribute

### Integration Points
- `processBlock` currently pass-through -- delay engine replaces this
- `createParameterLayout` currently returns empty layout -- all Phase 2 params go here
- `src/dsp/` directory exists (empty) -- delay line, tap reader, character processor go here
- `test/dsp/` directory for DSP unit tests

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 02-core-delay-engine*
*Context gathered: 2026-03-05*
