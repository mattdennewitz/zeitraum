# Architecture Patterns

**Domain:** JUCE multi-tap delay audio plugin with feedback matrix
**Researched:** 2026-03-05

## Recommended Architecture

Follow the proven pattern from the three-sisters project: a thin `AudioProcessor` shell that delegates all DSP to a dedicated engine class. The multi-tap delay is more complex (delay buffers, feedback matrix, modulation), so the engine decomposes into sub-components.

### High-Level Structure

```
PluginProcessor (JUCE AudioProcessor)
  |-- APVTS (parameter tree, DAW automation)
  |-- DelayEngine[2] (one per stereo channel)
  |     |-- DelayLine (circular buffer, shared write head)
  |     |-- TapReader[8] (fractional-sample read from delay line)
  |     |-- CharacterFilter (HF rolloff, bandwidth limiting per feedback pass)
  |     |-- ParameterSmoother (per-parameter, zipper-free)
  |-- FeedbackMatrix (NxN routing with cross-channel gains)
  |-- CrossChannelRouter (manages L/R feedback exchange)
  |-- PluginEditor (GUI)
        |-- TapPositionDisplay (visual tap layout)
        |-- FeedbackMatrixEditor (NxN gain grid)
        |-- PresetMixControls (odd/even/rising/falling)
        |-- LookAndFeel (custom styling)
```

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| **PluginProcessor** | JUCE lifecycle, parameter wiring, calls processBlock | APVTS, DelayEngine, FeedbackMatrix, PluginEditor |
| **APVTS** | Thread-safe parameter storage, DAW automation, state save/load | PluginProcessor (owner), PluginEditor (listener) |
| **DelayEngine** | Per-channel DSP: write to delay line, read taps, apply character | DelayLine, TapReader[], CharacterFilter |
| **DelayLine** | Circular buffer write/read with fractional-sample interpolation | DelayEngine (owner) |
| **TapReader** | Reads one tap position from the delay line with interpolation | DelayLine (reads from) |
| **FeedbackMatrix** | Computes feedback sum from tap outputs + preset mixes, applies gains | DelayEngine (reads tap outputs), CrossChannelRouter |
| **CrossChannelRouter** | Routes feedback between L and R channels | FeedbackMatrix, both DelayEngines |
| **CharacterFilter** | HF rolloff, bandwidth limiting, optional mild noise injection | DelayEngine (inline in feedback path) |
| **ParameterSmoother** | Per-sample smoothing for any parameter target | All DSP components |
| **PluginEditor** | GUI rendering, parameter binding via APVTS | APVTS (reads/writes params), PluginProcessor |

## Data Flow

### Per-Sample Audio Processing (within processBlock)

```
For each sample in buffer:

  1. Read smoothed parameters (tap positions, gains, feedback amounts)

  2. For each channel (L, R):
     a. Compute feedback sum from FeedbackMatrix
        - Takes previous-sample tap outputs from both channels
        - Applies NxN gain matrix
        - Sums to single feedback value per channel
     b. Mix input sample + feedback sum
     c. Apply CharacterFilter to feedback contribution (HF rolloff)
     d. Write mixed sample to DelayLine
     e. Read 8 tap positions from DelayLine (fractional-sample interpolation)
     f. Apply per-tap level controls
     g. Compute preset mixes (odd, even, rising, falling)
     h. Sum tap outputs to channel output

  3. CrossChannelRouter:
     - Takes tap outputs from both L and R engines
     - Feeds cross-channel amounts into each channel's FeedbackMatrix
     - Applied on next sample (one-sample delay in feedback path is expected)
```

### Parameter Flow (UI to DSP)

```
GUI slider/knob
  -> APVTS atomic parameter update (lock-free)
  -> processBlock reads atomic<float>* cached pointers
  -> ParameterSmoother interpolates to target (per-sample)
  -> DSP component receives smoothed value
```

### State Flow (DAW save/load)

```
Save: APVTS -> XML -> MemoryBlock -> DAW project file
Load: DAW project file -> MemoryBlock -> XML -> APVTS -> snap smoothers to loaded values
```

## Key Architecture Decisions

### 1. One DelayLine Per Channel, Not Per Tap

The hardware uses a single shared delay line (shift register / DRAM) with taps reading at different offsets. Replicate this: one circular buffer per channel, 8 read positions.

**Why:** Faithful to the hardware model. Tap positions are offsets along the same buffer, not independent delay times. Changing the base delay time shifts all taps proportionally. Simpler memory model.

### 2. FeedbackMatrix Lives Outside DelayEngine

The feedback matrix needs access to tap outputs from BOTH channels (cross-channel routing). It cannot live inside a single-channel engine.

**Structure:**
```cpp
class FeedbackMatrix {
    // feedback_gain[source_tap][destination_input] -- NxN
    // Plus preset-mix rows (odd, even, rising, falling) as virtual sources
    // Plus cross-channel source columns
    float gains[NUM_SOURCES][NUM_DESTINATIONS];

    // Returns feedback sum for a given destination channel
    float computeFeedback(int destChannel,
                          const std::array<float, 8>& tapsL,
                          const std::array<float, 8>& tapsR,
                          const PresetMixes& mixesL,
                          const PresetMixes& mixesR);
};
```

**Source count:** 8 taps + 4 preset mixes = 12 sources per channel, x2 channels = 24 possible sources. Destination is simply the delay line input for each channel (2 destinations). So the matrix is 24x2, not a full NxN. This is manageable.

### 3. Fractional-Sample Interpolation for Smooth Modulation

When delay time is modulated (doppler/tape effects), the read position moves continuously. Use cubic (Hermite) interpolation for the tap readers to avoid aliasing artifacts.

```cpp
class TapReader {
    float readFractional(const DelayLine& line, float delaySamples) const {
        // Hermite 4-point interpolation
        int idx = static_cast<int>(delaySamples);
        float frac = delaySamples - idx;
        float y0 = line.readAt(idx + 1);
        float y1 = line.readAt(idx);
        float y2 = line.readAt(idx - 1);
        float y3 = line.readAt(idx - 2);
        // Hermite polynomial...
    }
};
```

**Alternative considered:** JUCE's `juce::dsp::DelayLine` provides this built-in with Lagrange or Thiran interpolation. Use it if sufficient; roll custom only if JUCE's implementation lacks needed control (e.g., reading multiple tap positions from one line efficiently). The JUCE `DelayLine` is designed for single-tap use -- calling `setDelay()` and `popSample()` per tap would work but may be less efficient than a custom circular buffer with direct indexed reads for 8 taps.

**Recommendation:** Custom circular buffer. Reading 8 taps from JUCE's `DelayLine` would require either 8 `DelayLine` instances sharing data (wasteful) or using the lower-level `read()` method. A simple circular buffer with Hermite interpolation is ~50 lines of code and gives full control.

### 4. Parameter Smoothing Strategy

Follow three-sisters pattern: dedicated `ParameterSmoother` class using one-pole lowpass (exponential smoothing). Target set from atomic parameter; per-sample `.next()` call produces smoothed value.

**Critical smoothing targets:**
- Tap positions (smoothing creates doppler pitch shift -- this is a FEATURE, not a bug)
- Feedback gains (avoid clicks on matrix changes)
- Tap levels (avoid clicks)
- Base delay time and multiplier (doppler artifacts desired here)

**Snap-to-target on preset load:** Call `snapToTarget()` on all smoothers when loading state to avoid long parameter sweeps on project open.

### 5. Character Processing Placement

The character filter (HF rolloff, bandwidth limiting) goes in the **feedback path**, not the direct signal path. Each time audio recirculates through the delay, it loses more high-frequency content -- this is what creates the natural decay character of analog delay/reverb.

```
input + feedback_sum -> [CharacterFilter] -> write to DelayLine
                                              |
                                        read taps -> output
```

Optional: mild noise injection at very low level (-80dB) into the delay line write, gated by signal presence, for analog character.

## Patterns to Follow

### Pattern 1: Engine-Per-Channel with Cross-Channel Coordination

Each channel has its own `DelayEngine` instance (own delay line, own tap readers, own character filter). The `FeedbackMatrix` and `CrossChannelRouter` sit at the `PluginProcessor` level and coordinate between the two engines.

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        // Update smoothed parameters (once per sample)
        updateSmoothedParams();

        // Compute feedback from previous sample's tap outputs
        float fbL = feedbackMatrix.computeFeedback(0, prevTapsL, prevTapsR, ...);
        float fbR = feedbackMatrix.computeFeedback(1, prevTapsL, prevTapsR, ...);

        // Process each channel
        auto tapsL = engineL.process(left[i], fbL);
        auto tapsR = engineR.process(right[i], fbR);

        // Store for next sample's feedback
        prevTapsL = tapsL;
        prevTapsR = tapsR;

        // Sum taps to output (with per-tap levels)
        left[i] = sumTaps(tapsL, tapLevels);
        right[i] = sumTaps(tapsR, tapLevels);
    }
}
```

### Pattern 2: Preset Mixes as Derived Values, Not Stored State

Odd/even/rising/falling mixes are computed from tap outputs each sample, not stored separately. They are virtual outputs derived from the 8 tap values:

```cpp
struct PresetMixes {
    float odd;      // taps 1,3,5,7 summed
    float even;     // taps 2,4,6,8 summed
    float rising;   // taps weighted 1/8, 2/8, ... 8/8
    float falling;  // taps weighted 8/8, 7/8, ... 1/8
};

PresetMixes computePresetMixes(const std::array<float, 8>& taps) {
    PresetMixes m;
    m.odd = taps[0] + taps[2] + taps[4] + taps[6];
    m.even = taps[1] + taps[3] + taps[5] + taps[7];
    m.rising = m.falling = 0.0f;
    for (int i = 0; i < 8; ++i) {
        m.rising += taps[i] * (i + 1) / 8.0f;
        m.falling += taps[i] * (8 - i) / 8.0f;
    }
    return m;
}
```

### Pattern 3: Delay Time = Base + Tap Offset

Total delay for tap N = `baseDelay * multiplier * tapPosition[N]`

Where `tapPosition[N]` is normalized 0..1 along the delay line. Equal spacing preset sets `tapPosition[N] = (N+1) / 8.0`. Free mode allows arbitrary positioning.

The base delay range (10-150ms) times the multiplier (1x-8x) gives the full range up to ~1.2s. Tap positions are fractional within this total range.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Per-Block Parameter Updates
**What:** Reading parameters once per processBlock call instead of per-sample.
**Why bad:** At 512-sample blocks and 44.1kHz, that's ~11.6ms between updates. Delay time changes will produce audible stepping/zipper noise. Feedback gain changes will click.
**Instead:** Use per-sample smoothed parameters. The ParameterSmoother pattern from three-sisters handles this correctly.

### Anti-Pattern 2: Shared Mutable State Between Audio and GUI Threads
**What:** GUI directly modifying DSP state or DSP exposing references to internal state.
**Why bad:** Data races, crashes, undefined behavior.
**Instead:** All communication through APVTS atomic parameters. GUI reads/writes APVTS; audio thread reads APVTS. For display data flowing DSP -> GUI (e.g., tap output levels for meters), use atomic writes from audio thread, polled by GUI on timer.

### Anti-Pattern 3: Allocating Memory in processBlock
**What:** Creating vectors, strings, or any heap-allocated objects in the audio callback.
**Why bad:** Memory allocation can block, causing audio dropouts.
**Instead:** Pre-allocate all buffers in `prepareToPlay()`. Use fixed-size arrays (`std::array`). The `DelayLine` circular buffer is allocated once at prepare time based on max delay length.

### Anti-Pattern 4: Unbounded Feedback Gain
**What:** Allowing feedback matrix gains to sum > 1.0 without limiting.
**Why bad:** Exponential blowup, speaker damage.
**Instead:** Apply a soft-limiter or tanh saturation on the feedback sum before writing to the delay line. This also adds musical character (analog circuits naturally saturate).

## File Organization

```
src/
  PluginProcessor.h / .cpp     -- JUCE AudioProcessor, parameter layout, processBlock
  PluginEditor.h / .cpp        -- GUI composition
  dsp/
    DelayEngine.h              -- Per-channel: owns DelayLine + TapReaders + CharacterFilter
    DelayLine.h                -- Circular buffer with fractional read
    TapReader.h                -- Reads single tap with Hermite interpolation
    FeedbackMatrix.h           -- NxN routing computation
    CrossChannelRouter.h       -- Stereo feedback coordination
    CharacterFilter.h          -- HF rolloff, bandwidth limiting
    ParameterSmoother.h        -- Copy from three-sisters (proven)
    PresetMixes.h              -- Odd/even/rising/falling computation
  ui/
    TapPositionDisplay.h       -- Visual tap layout
    FeedbackMatrixEditor.h     -- NxN gain grid UI
    PresetMixControls.h        -- Mix selection UI
    MultiTapLookAndFeel.h      -- Custom styling
```

## Suggested Build Order

Dependencies flow bottom-up. Build and test in this order:

### Layer 1: Standalone DSP primitives (no JUCE dependency beyond juce_dsp)
1. **ParameterSmoother** -- copy from three-sisters, proven
2. **DelayLine** -- circular buffer with write/read, fractional indexing
3. **TapReader** -- Hermite interpolation reader, tested against DelayLine
4. **CharacterFilter** -- one-pole or biquad HF rolloff

### Layer 2: Composition
5. **PresetMixes** -- pure function, trivially testable
6. **DelayEngine** -- composes DelayLine + TapReader[8] + CharacterFilter, mono processing
7. **FeedbackMatrix** -- takes tap arrays, returns feedback sums
8. **CrossChannelRouter** -- coordinates FeedbackMatrix across L/R

### Layer 3: JUCE integration
9. **PluginProcessor** -- wire APVTS parameters to engines + feedback matrix, processBlock
10. **Basic PluginEditor** -- minimal GUI with sliders for all parameters (functional but ugly)

### Layer 4: GUI refinement
11. **TapPositionDisplay** -- visual tap layout
12. **FeedbackMatrixEditor** -- NxN grid
13. **PresetMixControls** -- mix selection
14. **MultiTapLookAndFeel** -- final polish

**Rationale:** Layers 1-2 are pure DSP, testable with Catch2 unit tests using synthetic buffers. No audio hardware needed. Layer 3 makes it functional as a plugin. Layer 4 makes it usable. This ordering means the DSP core is solid before any GUI work begins.

## Scalability Considerations

| Concern | At 44.1kHz/512 buf | At 96kHz/64 buf | Notes |
|---------|---------------------|-----------------|-------|
| CPU per sample | ~8 interpolated reads + matrix multiply + filter = low | Same ops, 2x sample rate | Well within budget for stereo |
| Memory | ~106KB per channel (1.2s @ 44.1kHz * 4 bytes) | ~230KB per channel @ 96kHz | Trivial |
| Parameter updates | 8 tap positions + matrix gains = ~30 smoothers | Same count, 2x calls/sec | ParameterSmoother is a single multiply-add |
| Feedback stability | Soft limiter prevents blowup | Same | Essential regardless of scale |

## Sources

- Three-sisters project (`~/src/three-sisters/`) -- proven JUCE plugin architecture pattern with per-channel engine, APVTS parameter flow, ParameterSmoother, CMake build (HIGH confidence)
- JUCE AudioProcessor documentation and `juce::dsp::DelayLine` API (HIGH confidence, based on established JUCE patterns)
- Standard DSP knowledge: circular buffers, Hermite interpolation, one-pole smoothing, feedback stability (HIGH confidence, textbook material)
