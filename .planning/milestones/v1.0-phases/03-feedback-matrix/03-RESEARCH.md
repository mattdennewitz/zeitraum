# Phase 3: Feedback Matrix - Research

**Researched:** 2026-03-06
**Domain:** Audio DSP -- feedback routing, saturation/limiting, one-pole filtering
**Confidence:** HIGH

## Summary

Phase 3 adds a feedback routing matrix to the existing 8-tap delay engine. Twelve sources (8 individual taps + 4 preset mixes) each have independent feedback gain. All feedback signals sum into a single bus that passes through HP/LP filters (6dB/oct one-pole), then tanh-style soft clipping, then an energy-based limiter, before being added back to the delay line input. The core modification is to `DelayEngine::process()` -- tap outputs must be captured per-sample for feedback routing, and the feedback bus signal must be summed with the input before `pushSample`.

The DSP primitives are straightforward: one-pole filters (already established via `OnePoleSmooth` pattern), `std::tanh` for soft clipping, and an RMS envelope follower for the energy limiter. No external libraries are needed beyond what JUCE and the standard library already provide. The main complexity is architectural: threading the feedback signal through the per-sample inner loop correctly, managing 12+ new smoothed parameters without bloating the process loop, and ensuring stability under all gain combinations.

**Primary recommendation:** Build three new header-only DSP classes (`FeedbackMatrix`, `FeedbackFilter`, `FeedbackSaturator`), modify `DelayEngine` to expose per-sample tap outputs and accept a feedback signal, and add 16+ new APVTS parameters with cached atomic pointers following the established pattern.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- 12 feedback sources: 8 individual taps + 4 fixed preset mixes (Odd, Even, Rising, Falling)
- Each source has its own independent feedback gain (0-100%)
- All feedback sources sum into a single feedback bus that feeds back to the delay line input
- Preset mix definitions are fixed: Odd = taps 1,3,5,7 at equal level; Even = 2,4,6,8 at equal level; Rising = linear ramp 1->8; Falling = linear ramp 8->1
- Warm analog soft clip (tanh-style) on the feedback bus
- Energy-based limiter monitors RMS of the feedback bus; attenuates when sustained energy exceeds threshold
- At max feedback gain, sustained self-oscillation is possible -- signal sustains indefinitely but soft clipper prevents growth (dub delay behavior)
- Global HP and LP filters on the summed feedback bus (not per-source)
- Gentle one-pole filters (6dB/oct)
- Each filter has an on/off bypass switch
- Signal chain order: filters before saturation
- Preset mixes available both as feedback sources AND as output mix options
- Selecting a preset mix replaces the individual tap level parameters (knobs update to new values)
- Functional parameter display using GenericAudioProcessorEditor or minimal custom panel
- All 12 feedback gains, filter controls, and bypass toggles accessible

### Claude's Discretion
- Exact feedback filter frequency ranges (user wants both standard and wider ranges available; consider combined range or switchable)
- Energy-based limiter implementation details (RMS window size, threshold, attack/release)
- Soft clip curve specifics (exact tanh scaling)
- How preset mix output selection is exposed (parameter, button, etc.)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FDBK-01 | Feedback routing matrix where any tap or preset mix can be routed back to delay input with independent gain | FeedbackMatrix class with 12 gain parameters; per-sample tap capture in DelayEngine; feedback bus summation before pushSample |
| FDBK-02 | Feedback matrix includes saturation/limiting to prevent runaway oscillation | FeedbackSaturator class with tanh soft clip + RMS energy limiter; signal chain: filters -> saturator -> limiter |
| FDBK-03 | Feedback path includes highpass and lowpass filters for tonal shaping | FeedbackFilter class with bypassable one-pole HP/LP (6dB/oct); global on the summed bus |
| FDBK-04 | Interactive visual display of the feedback routing matrix | GenericAudioProcessorEditor exposes all parameters; full matrix GUI deferred to Phase 5 |
| MIX-02 | Preset mixes: odd taps, even taps, rising-level, falling-level | Fixed preset mix definitions applied to tap levels via output mix selector parameter |
| MIX-03 | Preset mixes available as feedback sources in routing matrix | 4 additional feedback gain parameters (FB_ODD, FB_EVEN, FB_RISING, FB_FALLING) that compute weighted sums of tap outputs |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Audio plugin framework, APVTS, DSP primitives | Already in use; provides DelayLine, ProcessSpec |
| C++ std library | C++17 | `std::tanh`, `std::exp`, `std::sqrt` for DSP math | Zero dependency; compiler-optimized intrinsics |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce::dsp::FirstOrderTPTFilter | JUCE 8 | One-pole HP/LP filter | Could use for feedback filters, but hand-rolling is simpler for bypassed one-pole and avoids JUCE dependency in DSP class |
| Catch2 | 3.7.1 | Unit testing feedback stability and filter behavior | All new DSP classes need tests |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-rolled one-pole filter | juce::dsp::FirstOrderTPTFilter | TPT filter is already used in CharacterProcessor; but for feedback filters we want JUCE-free header-only DSP, and a one-pole is trivial to implement correctly |
| std::tanh for soft clip | Polynomial approximation (x - x^3/3) | tanh is more accurate at extremes; polynomial cheaper but less warm. std::tanh is fine at audio rate for a single-point-per-sample operation |

## Architecture Patterns

### Recommended New Files
```
src/dsp/
  FeedbackMatrix.h      # Routes tap outputs through gains, sums to bus
  FeedbackFilter.h      # Bypassable one-pole HP + LP on feedback bus
  FeedbackSaturator.h   # tanh soft clip + RMS energy limiter
```

### Pattern 1: Feedback Signal Flow (Per-Sample Inner Loop)

**What:** The feedback path must be computed per-sample inside the existing channel loop in DelayEngine. Each sample: (1) read tap outputs, (2) compute feedback bus from weighted tap sum, (3) filter the bus, (4) saturate/limit the bus, (5) add feedback bus to input, (6) push to delay line.

**When to use:** Always -- this is the core modification.

**Example:**
```cpp
// Inside DelayEngine::process() per-channel, per-sample loop:
float inputSample = characterProcessor.process(ch, channelData[i], smoothedCharacter[i]);

// Read all tap outputs (store for feedback computation)
float tapOutputs[numTaps];
for (int t = 0; t < numTaps; ++t)
{
    bool isLastTap = (t == numTaps - 1);
    tapOutputs[t] = delayLine[ch].popSample(0, smoothedTapDelay[t][i], isLastTap);
}

// Compute feedback bus (FeedbackMatrix sums weighted tap outputs)
float feedbackBus = feedbackMatrix.process(tapOutputs, i);  // uses pre-smoothed gains

// Filter and saturate the feedback bus
feedbackBus = feedbackFilter.process(ch, feedbackBus);
feedbackBus = feedbackSaturator.process(feedbackBus);

// Push input + feedback to delay line
delayLine[ch].pushSample(0, inputSample + feedbackBus);

// Compute wet output from tap levels (unchanged from current)
float wetSample = 0.0f;
for (int t = 0; t < numTaps; ++t)
    wetSample += tapOutputs[t] * smoothedTapLevel[t][i];

channelData[i] = wetSample;
```

**Critical change from current code:** Currently `pushSample` happens BEFORE `popSample`. With feedback, we need the tap outputs from the PREVIOUS push to compute feedback for the CURRENT push. This means the loop order must change: pop first (reading from previously pushed data), then push (input + feedback from just-popped taps). This is the standard approach for feedback delay lines.

### Pattern 2: Preset Mix as Weighted Tap Sum

**What:** The 4 preset mixes (Odd, Even, Rising, Falling) are computed as fixed weighted sums of the 8 tap outputs. These serve dual purpose: feedback sources (with their own gain parameter) and output mix selectors (replacing individual tap levels).

**When to use:** Inside FeedbackMatrix for feedback routing; in DelayEngine or PluginProcessor for output mix selection.

**Example:**
```cpp
// Fixed weights (normalized so sum = 1.0 for unity gain)
static constexpr float oddWeights[8]     = {0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f};
static constexpr float evenWeights[8]    = {0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f};
static constexpr float risingWeights[8]  = {1/36.f, 2/36.f, 3/36.f, 4/36.f, 5/36.f, 6/36.f, 7/36.f, 8/36.f};
static constexpr float fallingWeights[8] = {8/36.f, 7/36.f, 6/36.f, 5/36.f, 4/36.f, 3/36.f, 2/36.f, 1/36.f};
```

### Pattern 3: One-Pole Filter for Feedback Path

**What:** Simple one-pole HP and LP filters at 6dB/oct with bypass switch. Different from OnePoleSmooth (which is a parameter smoother) -- these are audio filters with frequency-dependent coefficients.

**When to use:** On the summed feedback bus, before saturation.

**Example:**
```cpp
// One-pole lowpass: y[n] = y[n-1] + alpha * (x[n] - y[n-1])
// alpha = 1 - exp(-2*pi*fc / sampleRate)
//
// One-pole highpass: y[n] = (1 - alpha) * (y[n-1] + x[n] - x[n-1])
// Or equivalently: highpass = input - lowpass(input)

class FeedbackFilter
{
    // Per-channel state (dual mono)
    float lpState[2] = {};
    float hpState[2] = {};
    float hpPrev[2] = {};

    float lpAlpha = 0.0f;    // Smoothed per-sample
    float hpAlpha = 0.0f;
    bool lpBypassed = true;
    bool hpBypassed = true;

    float process(int ch, float input)
    {
        float out = input;
        if (!hpBypassed)
        {
            // DC-blocking highpass
            hpState[ch] = hpAlpha * (hpState[ch] + input - hpPrev[ch]);
            hpPrev[ch] = input;
            out = hpState[ch];
        }
        if (!lpBypassed)
        {
            lpState[ch] += lpAlpha * (out - lpState[ch]);
            out = lpState[ch];
        }
        return out;
    }
};
```

### Pattern 4: tanh Soft Clipping

**What:** Warm analog-style saturation using scaled tanh. The scaling factor controls how aggressively the signal is driven into saturation.

**Example:**
```cpp
// Drive controls how hard the signal hits the saturator
// At drive=1.0, signal passes through tanh(x) which soft-clips at +/-1
// Compensate output gain so quiet signals aren't boosted
float softClip(float input, float drive = 1.5f)
{
    return std::tanh(input * drive) / std::tanh(drive);
    // Division by tanh(drive) normalizes so small signals pass at unity
}
```

### Pattern 5: RMS Energy Limiter

**What:** Monitors RMS energy of the feedback bus over a short window. When sustained energy exceeds threshold, gain is reduced. Allows transient peaks (dub delay character) while preventing sustained runaway.

**Example:**
```cpp
class EnergyLimiter
{
    float rmsSquared = 0.0f;     // Running average of squared signal
    float gain = 1.0f;           // Current attenuation
    float rmsAlpha;              // Smoothing for RMS envelope
    float attackAlpha;           // How fast gain reduces
    float releaseAlpha;          // How fast gain recovers
    float threshold = 0.9f;      // RMS threshold (linear)

    float process(float input)
    {
        // Update RMS estimate (exponential moving average of squared signal)
        rmsSquared += rmsAlpha * (input * input - rmsSquared);
        float rms = std::sqrt(rmsSquared);

        // Compute target gain
        float targetGain = (rms > threshold) ? (threshold / rms) : 1.0f;

        // Smooth gain changes (fast attack, slow release)
        float alpha = (targetGain < gain) ? attackAlpha : releaseAlpha;
        gain += alpha * (targetGain - gain);

        return input * gain;
    }
};
```

### Anti-Patterns to Avoid
- **Feedback before popSample:** Must pop all taps first, THEN push (input + feedback). Current code pushes then pops -- this must be reversed for feedback to work.
- **Per-source filtering:** The user decided global HP/LP on the summed bus, not per-source. Do not create 12 filter instances.
- **Unsynchronized stereo limiting:** The energy limiter should use the same gain for both channels to avoid stereo image shifting. Either link the RMS detectors or use a max-of-both approach.
- **Allocating in the feedback loop:** All scratch buffers for smoothed feedback gains must be pre-allocated in `prepare()`, following the established pattern.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter smoothing | Custom smoother | `OnePoleSmooth` (existing) | Already proven, used for all params in Phase 2 |
| Delay line | Custom circular buffer | `juce::dsp::DelayLine` (existing) | Already in use, Lagrange interpolation included |
| Dry/wet mixing | Manual crossfade | `juce::dsp::DryWetMixer` (existing) | Already in use, handles smoothing |

**Key insight:** The feedback DSP primitives (one-pole filter, tanh, RMS follower) are genuinely simple enough to hand-roll as header-only classes. Using JUCE's TPT filter would add a dependency without benefit for a 6dB/oct one-pole.

## Common Pitfalls

### Pitfall 1: Push-Before-Pop Ordering
**What goes wrong:** If you push the input + feedback BEFORE popping tap outputs, you read the sample you just wrote (zero-sample feedback path), creating instant oscillation or incorrect delay timing.
**Why it happens:** The current code pushes then pops. Adding feedback requires reversing this.
**How to avoid:** Always pop all taps first, compute feedback from those outputs, then push (input + feedback). On the very first sample after prepare(), the delay line contains zeros so feedback is naturally zero.
**Warning signs:** Feedback at any non-zero gain causes immediate oscillation or DC offset.

### Pitfall 2: Feedback Gain Staging Blowup
**What goes wrong:** 12 feedback sources each at 100% gain, all summed, can produce 12x the signal level before saturation. The saturator sees enormous input and the output is essentially a square wave.
**Why it happens:** Naive summation of all feedback sources without gain normalization.
**How to avoid:** The tanh soft clipper handles this by design -- it compresses any input to [-1, +1]. But the energy limiter's threshold should be set relative to the post-saturation signal. Consider whether the individual feedback gains should be 0-100% of the tap output, or whether the total bus should be normalized. User decision says 0-100% per source with saturation handling the rest, so let the saturator do its job. Test with all gains at 100%.
**Warning signs:** Output is always clipped/squared when more than 2-3 feedback sources are active.

### Pitfall 3: Denormals in Feedback Loop
**What goes wrong:** Very quiet feedback signals (near-zero) can become denormalized floats, causing massive CPU spikes as the processor falls back to microcode.
**Why it happens:** One-pole filters and RMS followers holding tiny values that decay toward zero.
**How to avoid:** `juce::ScopedNoDenormals` is already at the top of processBlock. Also add a tiny DC offset or flush-to-zero check in the filter state updates: `if (std::abs(state) < 1e-15f) state = 0.0f;`
**Warning signs:** CPU usage spikes when feedback signal decays to silence.

### Pitfall 4: Filter Coefficient Updates Per-Sample
**What goes wrong:** Recomputing `exp(-2*pi*fc/sr)` every sample is expensive. With smoothed frequency parameters, the coefficient changes every sample.
**Why it happens:** Naive implementation recomputes transcendental functions in the inner loop.
**How to avoid:** Smooth the alpha coefficient directly rather than the frequency, or compute alpha once per block if frequency isn't being automated. Alternatively, pre-compute into scratch buffers like the existing delay smoothing pattern.
**Warning signs:** CPU usage significantly higher with feedback filters enabled.

### Pitfall 5: Stereo Limiter Pumping
**What goes wrong:** Independent energy limiters per channel cause the stereo image to shift as one channel is attenuated more than the other.
**Why it happens:** Left and right channels have different content, so RMS levels differ.
**How to avoid:** Link the stereo channels: compute RMS as max(rmsL, rmsR) or average, apply the same gain reduction to both channels.
**Warning signs:** Stereo image collapses or shifts during heavy feedback.

### Pitfall 6: State Persistence Version Compatibility
**What goes wrong:** Adding 16+ new parameters breaks state recall from Phase 2 sessions.
**Why it happens:** APVTS replaceState() with XML that doesn't contain the new parameters.
**How to avoid:** APVTS handles this gracefully -- missing parameters fall back to their default values. Increment `pluginVersion` attribute to 2 in getStateInformation. In setStateInformation, version 1 XML will simply use defaults for new feedback parameters. Test round-trip with Phase 2 state.
**Warning signs:** Loading old sessions crashes or produces unexpected feedback settings.

## Code Examples

### New Parameter Layout Additions
```cpp
// 12 feedback gain parameters
for (int i = 1; i <= 8; ++i)
{
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_TAP" + juce::String(i), 1},
        "Feedback Tap " + juce::String(i),
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));
}

// Preset mix feedback gains
for (auto& name : {"ODD", "EVEN", "RISING", "FALLING"})
{
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{juce::String("FB_") + name, 1},
        juce::String("Feedback ") + name,
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));
}

// Feedback filter parameters
layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"FB_HP_FREQ", 1}, "Feedback HP Freq",
    juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f),
    20.0f, "Hz"));

layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"FB_LP_FREQ", 1}, "Feedback LP Freq",
    juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f),
    20000.0f, "Hz"));

layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"FB_HP_ON", 1}, "Feedback HP On", false));

layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"FB_LP_ON", 1}, "Feedback LP On", false));

// Output mix preset selector (0=Manual, 1=Odd, 2=Even, 3=Rising, 4=Falling)
layout.add(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID{"OUTPUT_MIX", 1}, "Output Mix",
    juce::StringArray{"Manual", "Odd", "Even", "Rising", "Falling"}, 0));
```

### Recommended Filter Frequency Ranges
```
HP: 20 Hz - 2000 Hz (default 20 Hz = effectively off)
LP: 200 Hz - 20000 Hz (default 20000 Hz = effectively off)
```
These ranges cover both "standard" feedback darkening (LP at 2-8 kHz) and wider experimental territory (LP down to 200 Hz for extreme muffling, HP up to 2 kHz for thin ringy feedback). The skew factor of 0.3 concentrates most of the knob range in the musically useful lower frequencies.

### Recommended Energy Limiter Parameters
```
RMS window: ~50ms (alpha = 1 - exp(-2*pi / (0.05 * sampleRate)))
Threshold: 0.85 (linear, ~-1.4 dB) -- allows moderate saturation before limiting kicks in
Attack: 5ms (fast, catches sustained energy quickly)
Release: 200ms (slow, allows feedback to breathe and sustain naturally)
```

### Recommended Soft Clip Scaling
```
drive = 1.5 (moderate -- signal hits tanh at 1.5x, giving gentle compression from ~0.6 onward)
output = tanh(input * drive) / tanh(drive)  -- unity gain for small signals
```
This provides audible warmth and harmonic content at moderate feedback levels while preventing hard clipping. At drive=1.5, signals above ~0.6 amplitude begin to compress noticeably.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Simple feedback gain (single knob) | Matrix routing (multiple sources with independent gain) | Hardware modular tradition (Buchla, Verbos) | Enables complex evolving textures |
| Hard clip feedback limiter | Soft clip + energy-based limiting | Analog modeling era | Musical saturation instead of harsh digital clipping |
| Fixed feedback filtering | Bypassable HP/LP in feedback path | Standard in modern delay plugins | Tonal control over repeat character |

## Open Questions

1. **Output mix preset interaction with automation**
   - What we know: Selecting a preset mix should update tap level knobs to the preset pattern
   - What's unclear: Should this be a momentary "apply" action or a continuous mode? If continuous, automating individual tap levels while a preset is selected creates a conflict.
   - Recommendation: Make it an "apply and switch to Manual" pattern -- selecting a preset sets the 8 tap levels then resets the selector to Manual. This avoids parameter conflicts and matches the user's description ("starting point, not a locked mode"). The OUTPUT_MIX parameter acts as a trigger, not a sustained mode.

2. **Filter coefficient smoothing strategy**
   - What we know: Filter frequency params need smoothing to avoid zipper noise. Computing `exp()` per sample is expensive.
   - What's unclear: Whether to smooth frequency and recompute alpha per block, or smooth alpha directly.
   - Recommendation: Compute alpha once per block from the smoothed frequency value. Block-rate updates (every 64-512 samples) are inaudible for filter sweeps at 6dB/oct slope. This avoids per-sample transcendental math.

3. **Feedback gain scaling: linear vs curved**
   - What we know: User specified 0-100% range. At 100%, self-oscillation should sustain.
   - What's unclear: Whether 0-100% maps linearly to 0.0-1.0 gain, or needs a curve for musical feel.
   - Recommendation: Linear 0-100% -> 0.0-1.0 gain. The saturation stage handles the nonlinear compression, so the gain itself can be linear. At 1.0 (100%), the feedback loop sustains indefinitely because the saturator limits growth but doesn't attenuate below unity for moderate signals.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 |
| Config file | CMakeLists.txt (FetchContent) |
| Quick run command | `make test` |
| Full suite command | `make test` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FDBK-01 | Feedback from any tap/mix routes back to input | unit | `make test` (FeedbackMatrixTests) | No -- Wave 0 |
| FDBK-02 | Saturation/limiting prevents runaway | unit | `make test` (FeedbackSaturatorTests) | No -- Wave 0 |
| FDBK-03 | HP/LP filters shape feedback tone | unit | `make test` (FeedbackFilterTests) | No -- Wave 0 |
| FDBK-04 | Matrix display accessible | integration | `make test` (PluginTests -- parameter exists) | Partial (PluginTests.cpp exists) |
| MIX-02 | Preset mixes available as output | unit | `make test` (DelayEngineTests) | No -- Wave 0 |
| MIX-03 | Preset mixes as feedback sources | unit | `make test` (FeedbackMatrixTests) | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `make test`
- **Per wave merge:** `make test`
- **Phase gate:** Full suite green + AU validation (`make validate`)

### Wave 0 Gaps
- [ ] `test/dsp/FeedbackMatrixTests.cpp` -- covers FDBK-01, MIX-03 (feedback routing, weighted sums)
- [ ] `test/dsp/FeedbackSaturatorTests.cpp` -- covers FDBK-02 (soft clip bounds, energy limiter stability)
- [ ] `test/dsp/FeedbackFilterTests.cpp` -- covers FDBK-03 (HP/LP frequency response, bypass)
- [ ] Extend `test/dsp/DelayEngineTests.cpp` -- covers FDBK-01 end-to-end (impulse with feedback produces repeats)
- [ ] Extend `test/PluginTests.cpp` -- covers FDBK-04, MIX-02 (new parameters exist, state round-trip with new params)

## Sources

### Primary (HIGH confidence)
- Existing codebase: `src/dsp/DelayEngine.h`, `src/dsp/OnePoleSmooth.h`, `src/dsp/TapReader.h`, `src/dsp/CharacterProcessor.h`, `src/PluginProcessor.h`, `src/PluginProcessor.cpp`
- JUCE 8 documentation: DelayLine, APVTS, FirstOrderTPTFilter patterns
- DSP fundamentals: one-pole filter coefficients, tanh saturation, RMS envelope following

### Secondary (MEDIUM confidence)
- Dub delay feedback behavior (sustained self-oscillation via soft clipping) -- well-documented in audio DSP literature and implemented in commercial plugins (Soundtoys EchoBoy, Valhalla Delay, u-he Colour Copy)

### Tertiary (LOW confidence)
- None -- all techniques in this phase are well-established DSP fundamentals

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all primitives are textbook DSP
- Architecture: HIGH -- feedback delay lines are thoroughly understood; codebase patterns are clear and established
- Pitfalls: HIGH -- push/pop ordering and gain staging are the classic feedback delay pitfalls; documented from direct codebase analysis

**Research date:** 2026-03-06
**Valid until:** 2026-04-06 (stable domain, no moving targets)
