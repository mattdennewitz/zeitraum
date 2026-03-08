# Phase 3: Feedback Matrix - Context

**Gathered:** 2026-03-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Route any combination of tap outputs and preset mixes back into the delay input with independent gain, producing complex evolving textures. Includes feedback path filtering, saturation/stability safeguards, preset mix definitions, and a functional parameter display. Full GUI polish is Phase 5.

</domain>

<decisions>
## Implementation Decisions

### Routing topology
- 12 feedback sources: 8 individual taps + 4 fixed preset mixes (Odd, Even, Rising, Falling)
- Each source has its own independent feedback gain (0-100%)
- All feedback sources sum into a single feedback bus that feeds back to the delay line input
- Preset mix definitions are fixed: Odd = taps 1,3,5,7 at equal level; Even = 2,4,6,8 at equal level; Rising = linear ramp 1->8; Falling = linear ramp 8->1

### Saturation & stability
- Warm analog soft clip (tanh-style) on the feedback bus -- gradual compression that adds warmth and harmonics
- Energy-based limiter monitors RMS of the feedback bus; attenuates when sustained energy exceeds threshold (allows transient peaks)
- At max feedback gain, sustained self-oscillation is possible -- signal sustains indefinitely but the soft clipper prevents growth (dub delay behavior)
- Saturation is audible and part of the sound design; no visual limiter indicator in this phase

### Feedback path filters
- Global HP and LP filters on the summed feedback bus (not per-source)
- Gentle one-pole filters (6dB/oct) -- subtle, musical darkening/thinning per repeat
- Each filter has an on/off bypass switch
- Frequency ranges: Claude's discretion (user wants both the standard and wider ranges available)
- Signal chain order: filters before saturation -- filtered signal hits the saturator cleaner

### Preset mixes as output
- Preset mixes (Odd, Even, Rising, Falling) available both as feedback sources AND as output mix options
- Selecting a preset mix replaces the individual tap level parameters (knobs update to new values)
- User can then tweak from the preset pattern -- it's a starting point, not a locked mode

### Matrix display
- Functional parameter display using GenericAudioProcessorEditor or minimal custom panel
- All 12 feedback gains, filter controls, and bypass toggles accessible
- Functional but not polished -- Phase 5 creates the beautiful interactive matrix

### Claude's Discretion
- Exact feedback filter frequency ranges (user wants both standard and wider ranges available; consider combined range or switchable)
- Energy-based limiter implementation details (RMS window size, threshold, attack/release)
- Soft clip curve specifics (exact tanh scaling)
- How preset mix output selection is exposed (parameter, button, etc.)

</decisions>

<specifics>
## Specific Ideas

- Dub delay is the reference for feedback behavior -- infinite repeats at max feedback, sustained but never growing
- Verbos Multi-Delay Processor and Buchla 288 are the hardware inspiration for the routing topology
- Feedback filters should each have a bypass switch so they can be turned off independently

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `OnePoleSmooth` (src/dsp/OnePoleSmooth.h): One-pole smoother pattern -- can inform the feedback filter implementation (though filters need different coefficients than parameter smoothing)
- `CharacterProcessor` (src/dsp/CharacterProcessor.h): Already applies HF roll-off to delay input; feedback filters are a separate stage in the feedback path
- `DelayEngine` (src/dsp/DelayEngine.h): Core delay loop needs modification -- currently push input then sum taps. Must add feedback bus summation before pushing to delay line
- `TapReader` (src/dsp/TapReader.h): Per-tap output already computed in the inner loop; tap outputs need to be captured for feedback routing

### Established Patterns
- Header-only DSP classes in src/dsp/ -- feedback matrix, filters, and saturation should follow this pattern
- APVTS with cached atomic parameter pointers -- 12 new feedback gain params + filter freq/bypass params
- OnePoleSmooth for all parameter smoothing (feedback gains must be smoothed to avoid zipper noise)
- Pre-allocated scratch buffers for per-sample processing (no audio-thread allocation)

### Integration Points
- `DelayEngine::process()` inner loop: feedback signal must be summed with input before `pushSample`
- `PluginProcessor::createParameterLayout()`: Add 12 feedback gain params, 2 filter freq params, 2 filter bypass params, preset mix selector
- `PluginProcessor::processBlock()`: Read feedback params, pass to DelayEngine
- State persistence: feedback gains, filter settings, and preset mix state must save/restore with session

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 03-feedback-matrix*
*Context gathered: 2026-03-06*
