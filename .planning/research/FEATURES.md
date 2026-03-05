# Feature Landscape

**Domain:** Multi-tap delay audio plugin (VST3/AU/CLAP)
**Researched:** 2026-03-05
**Confidence:** MEDIUM (based on training data knowledge of hardware modules and plugin ecosystem; web search unavailable for verification)

## Reference Hardware Analysis

### Verbos Multi-Delay Processor
- 8 equally-spaced taps along a single delay line
- Multiplier knob scales all tap times proportionally
- Per-tap level controls
- Preset mix outputs: all taps, odd taps, even taps, rising level, falling level
- Feedback from mix outputs back to input
- ~10-150ms base delay range, multiplier extends total
- BBD/analog character with bandwidth limiting

### Buchla 288 Time Domain Processor
- 8 taps with programmable positions (not fixed equal spacing)
- 10ms quantized tap positioning via patch programming
- Per-tap level/output
- More flexible routing than Verbos
- Designed for complex time-domain transformations, not just echo
- Short delay focus: comb filtering, phasing, resonant effects

### Common Ground (What Makes These Special)
Both use a shared serial delay line architecture (not independent delay buffers). The taps read from positions along a single delay line. This is fundamentally different from most modern multi-tap delays which use independent delay engines per tap. The shared line means tap interactions create complex comb filtering and resonance patterns, especially with feedback.

## Competitive Plugin Landscape

### Valhalla Delay
- Dual-engine delay (tape/digital/BBD modes per engine)
- Tempo sync with note divisions and dotted/triplet
- Ducking (delay ducks when input is present)
- Diffusion (smears repeats toward reverb-like)
- Per-engine filtering (HP/LP)
- Modulation (rate/depth on delay time)
- Age control (adds degradation/character)
- Excellent CPU efficiency

### Soundtoys EchoBoy
- 30+ echo styles modeled after classic hardware
- Tempo sync and free-running modes
- Rhythm mode (pattern-based multi-tap with up to 16 taps)
- Saturation and analog character per style
- Feel control (push/pull timing relative to grid)
- Groove/swing on rhythm patterns
- Low/high cut filtering on feedback path

### FabFilter Timeless 3
- 2 independent delay lines with flexible routing
- Per-delay filters (multimode, resonant)
- Extensive modulation system (drag-and-drop modulators)
- Tempo sync with visual feedback
- Freeze/infinite feedback mode
- Ping-pong, serial, parallel routing
- Modulation sources: LFOs, envelopes, XY controllers, MIDI

### Other Notable References
- **Eventide UltraTap:** Up to 64 taps, tap patterns, slurm (smear), chop (rhythmic gating)
- **Audio Damage Ratshack Reverb:** BBD-inspired multi-tap delay/reverb hybrid
- **u-he Colour Copy:** BBD-modeled delay with chorus/vibrato, 3 delay lines
- **Arturia Delay TAPE-201:** Tape modeling, wow/flutter, spring reverb

## Table Stakes

Features users expect from any modern delay plugin. Missing any of these means the plugin feels unfinished.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Multiple delay taps (at least 4-8) | Core promise of the plugin | High | 8 taps per the hardware references |
| Per-tap level control | Basic mixing capability | Low | Simple gain per tap |
| Wet/dry mix | Every delay plugin has this | Low | Global wet/dry knob |
| Feedback control | Fundamental delay behavior | Low | At minimum a global feedback amount |
| Tempo sync | DAW integration expectation | Medium | BPM sync with note divisions (1/4, 1/8, dotted, triplet) |
| Free-running (ms) mode | Not all use cases are tempo-locked | Low | Manual delay time in milliseconds |
| Delay time range covering short to long | Usability across contexts | Low | ~10ms to ~1.2s covers comb filtering through rhythmic echo |
| Highpass/lowpass filtering in delay path | Tonal shaping of repeats | Medium | At minimum HP and LP per feedback path |
| Stereo output | Standard for DAW plugins | Medium | Already planned; stereo in/out |
| Bypass/enable | DAW standard | Low | Host-managed bypass |
| All parameters automatable | DAW integration standard | Medium | JUCE parameter system handles this |
| Preset system | Users expect save/recall | Medium | JUCE provides XML state save/restore |
| Smooth parameter changes | No clicks, zippers, or glitches | High | Requires parameter smoothing throughout |
| Visual feedback of tap positions/timing | Users need to see what they're hearing | Medium | At minimum a tap position display |

## Differentiators

Features that set this plugin apart from the crowded delay market. These are the competitive advantage.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Shared serial delay line architecture | Authentic to hardware references; creates tap interactions and comb filtering that independent-buffer designs cannot | High | Core architectural decision, already planned. This IS the product. |
| Full NxN feedback routing matrix | No other plugin exposes this level of feedback routing flexibility; enables self-oscillating networks, complex resonance patterns | Very High | Already planned. The primary differentiator. Most plugins offer at most ping-pong or serial/parallel. |
| Cross-channel feedback routing | Stereo feedback networks create spatial movement impossible with standard stereo delays | High | Already planned. Feeds L taps back to R input and vice versa. |
| Preset tap patterns (odd/even/rising/falling) | Quick access to musically useful subsets, directly from the Verbos | Medium | Already planned. Pre-mixed tap combinations as outputs or blend targets. |
| Free tap positioning with equal-spacing as preset | Best of both Verbos (equal) and Buchla 288 (free); more flexible than either | Medium | Already planned. Equal spacing is one preset among many. |
| Doppler/tape artifacts on time modulation | When delay time changes, pitch shifts naturally like tape speed change; most digital delays crossfade or click | High | Already planned. Requires careful interpolation to avoid artifacts while preserving the analog feel. |
| Analog character approximation | Bandwidth limiting, mild noise, HF roll-off in delay path gives warmth without full circuit modeling cost | Medium | Already planned. Subtle but important for feel. |
| Tap time quantization (10ms steps) | Directly from Buchla 288; enables deliberate comb-filter tuning | Low | Already planned. Optional quantization grid. |
| Multiplier dial for proportional scaling | From Verbos; scales all tap times together while preserving ratios | Medium | Already planned. Unique interaction model. |
| Per-tap mute/solo | Quick auditioning of individual taps in a complex pattern | Low | Not in PROJECT.md but high value, low cost |
| Feedback matrix visualization | Visual display of the routing matrix; critical for usability of the NxN matrix | High | Without this, the NxN matrix is unusable. Must be clear and interactive. |

## Anti-Features

Features to explicitly NOT build. These either conflict with the design philosophy, add unjustified complexity, or dilute the product identity.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Independent delay buffers per tap | Defeats the entire architectural concept; standard multi-tap delays already do this | Shared serial delay line is the core identity |
| Granular/spectral processing | Scope creep; different product category entirely | Keep focus on time-domain delay processing |
| Built-in reverb | Scope creep; users have reverb plugins | The dense multi-tap patterns already approach reverb-like textures naturally |
| Pattern sequencer / step sequencer | Overcomplicates the interface; EchoBoy and UltraTap own this space | Preset tap patterns (odd/even/rising/falling) plus free positioning covers the need |
| Hardware skeuomorphic GUI | Already decided against in PROJECT.md; doesn't fit DAW aesthetic | Clean/modern GUI that fits alongside stock DAW plugins |
| MIDI note input / pitched delay | Different product (pitched delay / Karplus-Strong); scope creep | Comb-filter effects emerge naturally from short delay times |
| Convolution-based processing | Wrong paradigm; this is a real-time feedback system | Keep algorithmic delay processing |
| Modulation matrix (LFO/envelope/XY) | Significant UI and DSP complexity; FabFilter Timeless owns this space | Simple rate/depth modulation on delay time. DAW automation covers complex modulation needs. |
| Sidechain input for ducking | Nice-to-have but not core; adds routing complexity | Defer to v2 if demanded. Users can use a separate ducker plugin. |
| Oversampling | Delay effects don't typically alias; unnecessary CPU cost | Standard sample rate processing is fine for delay |
| Multi-band processing | Scope creep; different product | Single full-bandwidth delay path with filtering |
| Standalone application mode | Out of scope per PROJECT.md | Plugin formats only (VST3/AU/CLAP) |

## Feature Dependencies

```
Shared Delay Line (core DSP)
  +-- Per-tap level controls
  +-- Free tap positioning
  |     +-- Equal-spacing preset
  |     +-- Tap time quantization (10ms grid)
  +-- Multiplier dial (scales all positions)
  +-- Doppler artifacts (requires interpolation in delay line)
  +-- Analog character (filtering applied in delay path)

Feedback System
  +-- Global feedback amount
  +-- NxN feedback routing matrix
  |     +-- Feedback matrix visualization (UI)
  |     +-- Cross-channel feedback routing
  +-- Filtering in feedback path (HP/LP)

Preset Mixes (odd/even/rising/falling)
  +-- Per-tap level controls (prerequisite)
  +-- Feedback from preset mixes back to input

Tempo Sync
  +-- Host BPM detection
  +-- Note division mapping to tap positions
  +-- Multiplier interaction with sync'd values

GUI
  +-- Tap position visualization
  +-- Feedback matrix visualization
  +-- Per-tap controls display
  +-- Preset mix controls

Plugin Infrastructure
  +-- Parameter system (JUCE AudioProcessorValueTreeState)
  +-- State save/restore (presets)
  +-- DAW automation exposure
  +-- VST3/AU/CLAP format wrappers
```

## MVP Recommendation

The MVP should establish the core identity -- the shared delay line with feedback matrix -- before adding polish features.

**Prioritize (Phase 1 - Core):**
1. Shared serial delay line with 8 taps (the architectural foundation)
2. Per-tap level controls and mix output
3. Basic feedback (at least global feedback, ideally simplified matrix)
4. Wet/dry mix
5. Free-running delay time with multiplier

**Prioritize (Phase 2 - Identity):**
1. Full NxN feedback routing matrix with visualization
2. Free tap positioning with equal-spacing preset
3. Cross-channel feedback (stereo)
4. Preset tap mixes (odd/even/rising/falling)
5. Doppler/tape artifacts on time changes

**Prioritize (Phase 3 - Polish):**
1. Tempo sync with note divisions
2. Analog character (bandwidth limiting, noise, HF roll-off)
3. Tap time quantization (10ms steps)
4. Filtering in feedback path (HP/LP)
5. Clean/modern GUI refinement

**Defer:**
- Per-tap mute/solo: Low complexity but not core identity. Add when GUI is mature.
- Sidechain ducking: v2 feature if user demand warrants it.
- Advanced modulation: DAW automation covers this. Defer unless users demand it.

## Sources

- Training data knowledge of Verbos Multi-Delay Processor, Buchla 288 Time Domain Processor (MEDIUM confidence -- well-documented modules but could not verify current specs via web)
- Training data knowledge of Valhalla Delay, Soundtoys EchoBoy, FabFilter Timeless 3, Eventide UltraTap feature sets (MEDIUM confidence -- major plugins with stable feature sets, unlikely to have changed significantly)
- PROJECT.md requirements and constraints (HIGH confidence -- direct source)
- Web search was unavailable for verification; all findings based on training data
