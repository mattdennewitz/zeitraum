# Phase 2: Core Delay Engine - Research

**Researched:** 2026-03-05
**Domain:** Real-time audio DSP -- multi-tap delay line with analog character
**Confidence:** HIGH

## Summary

This phase builds the sonic core of Zeitraum: a shared stereo delay line with 8 independently positioned taps, per-tap levels, wet/dry mixing, delay time scaling, tap quantization, and BBD-inspired analog character. The implementation leverages JUCE 8's `juce::dsp::DelayLine` for the circular buffer with Lagrange3rd interpolation (best balance of quality and modulation friendliness), `juce::dsp::FirstOrderTPTFilter` for the cumulative HF roll-off character, and `juce::dsp::DryWetMixer` for mix control. All DSP helper classes should be header-only in `src/dsp/`, JUCE-free where possible, with per-sample parameter smoothing using the one-pole exponential formula specified in CLAUDE.md.

**Primary recommendation:** Use `juce::dsp::DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd>` as the shared delay buffer (one per channel), reading 8 taps via `popSample(channel, delaySamples, false)` (updateReadPointer=false for multi-tap), with a custom header-only `OnePoleSmooth` class for all parameter smoothing, and `FirstOrderTPTFilter` in the feedback/character path for cumulative HF darkening.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- Analog character: Subtle vintage BBD style -- mild HF loss + slight noise floor, cleaner than tape but not pristine
- Single "Character" knob that scales coloring from clean to full vintage (one parameter, not separate HF/saturation controls)
- Coloring applies per-repeat (cumulative) -- each pass through the delay line adds more color, so echoes progressively darken
- Subtle noise floor included, scaled by the Character knob -- only audible in quiet passages or with high feedback
- Tap positioning: Ratio-based, each tap has a position from 0.0-1.0 along the delay line
- Actual delay time = position * (base delay * multiplier), so taps scale proportionally when base time changes
- Quantization (10ms steps) snaps the actual delay time, not just the display
- Tap overlap is allowed -- taps can share the same position (comb filtering/doubling effects intentionally)
- Default preset: even distribution at 1/8, 2/8, 3/8... 8/8 of the delay line

### Claude's Discretion
- Parameter ranges and curves (base delay 10-150ms range, multiplier range, level curves linear vs dB)
- Smoothing time constants for parameter changes (within the constraint of no clicks/zipper noise)
- DSP implementation details (interpolation method for delay line, filter topology for character)
- Tap preset save/recall mechanism

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CORE-01 | 8 delay taps along a shared serial delay line (stereo) | DelayLine with popSample multi-tap pattern; dual-mono (one DelayLine per channel) |
| CORE-02 | Base delay time 10-150ms | AudioParameterFloat with skewed range; smoothed per-sample |
| CORE-03 | Multiplier dial scales all tap times (total up to ~5s) | Multiplier parameter 1x-33x; max buffer = 150ms * 33 * 96kHz = ~475,200 samples |
| CORE-04 | Per-tap individual level control | 8 float parameters TAP1_LEVEL..TAP8_LEVEL, 0.0-1.0, smoothed per-sample |
| CORE-05 | Free tap positioning along the delay line | 8 float parameters TAP1_POS..TAP8_POS, 0.0-1.0 ratio, smoothed per-sample |
| CORE-06 | Equal spacing as default tap preset | Default positions: 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 |
| CORE-07 | Tap time quantization to 10ms increments | Quantize toggle parameter + snap logic: round(timeSec / 0.01) * 0.01 |
| CORE-08 | Tap time presets save/recall | ValueTree-based preset storage in plugin state XML |
| CORE-09 | Global wet/dry mix | DryWetMixer or manual mix parameter, 0-100% |
| MIX-01 | Stereo operation | Dual-mono processing: independent DelayLine per L/R channel |
| INTG-04 | Analog character: bandwidth limiting, mild noise, HF roll-off | FirstOrderTPTFilter lowpass in delay path + noise injection, scaled by CHARACTER param |
| GUI-04 | Smooth parameter changes (no clicks/zipper noise) | One-pole exponential smoother on all audio-thread parameters, 5-15ms time constants |
| INFR-04 | Glitch-free at 64-512 sample buffer sizes | Pre-allocate max buffer in prepareToPlay; no allocation in processBlock |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::dsp::DelayLine | JUCE 8.0.12 | Shared circular delay buffer | Built-in interpolation, multi-tap via popSample, already linked |
| juce::dsp::FirstOrderTPTFilter | JUCE 8.0.12 | HF roll-off for analog character | TPT structure safe for modulation, per-sample processing, no artifacts |
| juce::dsp::DryWetMixer | JUCE 8.0.12 | Wet/dry mix with proper gain staging | Handles latency compensation and multiple mixing rules |
| juce::AudioProcessorValueTreeState | JUCE 8.0.12 | Parameter management | Already in use from Phase 1; lock-free, DAW-automatable |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce::dsp::ProcessSpec | JUCE 8.0.12 | DSP initialization context | Pass to all DSP processors in prepareToPlay |
| juce::Random | JUCE 8.0.12 | Noise floor generation | BBD noise character -- lightweight PRNG suitable for audio |
| Catch2 | v3.7.1 | Unit testing DSP classes | Already configured; test header-only DSP in test/dsp/ |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| juce::dsp::DelayLine | Custom circular buffer | DelayLine already handles wrapping, interpolation, multi-channel -- no reason to hand-roll |
| FirstOrderTPTFilter | IIR::Filter with makeFirstOrderLowPass | IIR::Filter can produce artifacts when cutoff modulated; TPT is modulation-safe |
| juce::SmoothedValue | Custom one-pole smoother | SmoothedValue uses linear/multiplicative ramp, not the exponential one-pole from CLAUDE.md; custom matches project convention |
| DryWetMixer | Manual mix math | DryWetMixer handles edge cases (latency comp, gain laws); worth using for correctness |

## Architecture Patterns

### Recommended Project Structure
```
src/
  PluginProcessor.h/cpp     # APVTS params, processBlock orchestration
  PluginEditor.h/cpp         # GenericAudioProcessorEditor (unchanged)
  dsp/
    DelayEngine.h            # Top-level: owns DelayLines, TapReaders, CharacterProcessor
    TapReader.h              # Per-tap: position, level, smoothing
    CharacterProcessor.h     # BBD character: lowpass + noise, cumulative per-repeat
    OnePoleSmooth.h          # One-pole exponential parameter smoother
test/
  PluginTests.cpp            # Integration: processor lifecycle, state round-trip with params
  dsp/
    DelayEngineTests.cpp     # DelayEngine: tap output, timing accuracy
    TapReaderTests.cpp       # TapReader: position math, quantization, smoothing
    CharacterProcessorTests.cpp  # Character: HF attenuation, noise floor
    OnePoleSmoothTests.cpp   # Smoother: convergence, time constant accuracy
```

### Pattern 1: Multi-Tap Read from Shared DelayLine
**What:** Use a single DelayLine per channel, push once per sample, pop 8 times at different delays with updateReadPointer=false
**When to use:** Always -- this is the core architecture
**Example:**
```cpp
// Source: JUCE 8.0.12 juce_DelayLine.h popSample documentation
// In processBlock, per sample, per channel:
delayLine.pushSample(channel, inputSample);

float wetSum = 0.0f;
for (int tap = 0; tap < 8; ++tap)
{
    float delaySamples = tapPosition[tap] * baseDelaySamples * multiplier;
    // Last tap updates read pointer, others don't
    bool updatePtr = (tap == 7);
    float tapSample = delayLine.popSample(channel, delaySamples, updatePtr);
    wetSum += tapSample * tapLevel[tap];
}
```

### Pattern 2: One-Pole Exponential Smoother (JUCE-free)
**What:** Per-sample parameter smoothing matching CLAUDE.md specification
**When to use:** All parameter changes on audio thread
**Example:**
```cpp
// Source: CLAUDE.md Parameter Smoothing section
class OnePoleSmooth {
public:
    void setTargetValue(float newTarget) { target = newTarget; }
    void reset(float value) { current = target = value; }
    void setSampleRate(double sr) { sampleRate = sr; recalcAlpha(); }
    void setTimeMs(float ms) { timeMs = ms; recalcAlpha(); }

    float getNextValue() {
        current += alpha * (target - current);
        return current;
    }

    bool isSmoothing() const {
        return std::abs(target - current) > 1e-6f;
    }

private:
    void recalcAlpha() {
        alpha = 1.0f - std::exp(-6.2831853f / (timeMs * 0.001f * static_cast<float>(sampleRate)));
    }
    float current = 0.0f, target = 0.0f, alpha = 1.0f;
    float timeMs = 10.0f;
    double sampleRate = 44100.0;
};
```

### Pattern 3: Cumulative Character Processing
**What:** Apply HF roll-off and noise at the point where samples enter the delay line, so each feedback pass darkens further
**When to use:** BBD-style cumulative coloring
**Example:**
```cpp
// Character processor sits between input and delay line push
// characterAmount from 0.0 (clean) to 1.0 (full vintage)
float processCharacter(int channel, float sample, float characterAmount) {
    // Interpolate lowpass cutoff: 20kHz (clean) to ~4kHz (full BBD)
    float cutoff = 20000.0f - characterAmount * 16000.0f;
    lpFilter.setCutoffFrequency(cutoff);
    float filtered = lpFilter.processSample(channel, sample);

    // Add subtle noise floor scaled by character
    float noise = (random.nextFloat() * 2.0f - 1.0f) * characterAmount * noiseLevel;
    return filtered + noise;
}
```

### Pattern 4: Tap Quantization
**What:** Snap actual delay time to 10ms grid when quantization is enabled
**When to use:** When QUANTIZE parameter is on
**Example:**
```cpp
float getQuantizedDelay(float positionRatio, float baseDelayMs, float multiplier, bool quantize) {
    float delayMs = positionRatio * baseDelayMs * multiplier;
    if (quantize) {
        delayMs = std::round(delayMs / 10.0f) * 10.0f;
        delayMs = std::max(delayMs, 10.0f); // Minimum 10ms
    }
    return delayMs;
}
```

### Anti-Patterns to Avoid
- **Separate delay buffers per tap:** Defeats the shared serial delay line architecture. One buffer, 8 read positions.
- **Reading parameters with getParameter()->getValue():** Use cached getRawParameterValue pointers + .load()
- **Allocating in processBlock:** Pre-allocate everything in prepareToPlay. No vectors, no strings.
- **Using juce::SmoothedValue for exponential smoothing:** It does linear or multiplicative ramp, not the one-pole exponential the project specifies.
- **Updating read pointer on every popSample:** For multi-tap, only the last pop per sample should advance the read pointer, otherwise earlier taps corrupt the read position for later ones.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Circular delay buffer | Custom ring buffer with wrapping | juce::dsp::DelayLine | Handles interpolation, wrapping, multi-channel; tested and optimized |
| Wet/dry mixing | Manual gain math | juce::dsp::DryWetMixer | Proper gain laws (sin3dB etc.), latency compensation, SmoothedValue built in |
| HF roll-off filter | Manual biquad coefficients | juce::dsp::FirstOrderTPTFilter | TPT structure prevents modulation artifacts; per-sample processing built in |
| Parameter thread safety | Manual atomics | APVTS + getRawParameterValue | Lock-free, DAW-automatable, state persistence built in |

**Key insight:** JUCE's juce_dsp module provides exactly the building blocks needed. The custom work is in the TapReader (position/quantization math), DelayEngine (orchestration), and CharacterProcessor (BBD-specific tuning) -- not in the low-level buffer or filter implementations.

## Common Pitfalls

### Pitfall 1: DelayLine Maximum Size Underestimate
**What goes wrong:** Buffer too small for max delay time at high sample rates, causing wrapping artifacts or crashes
**Why it happens:** Not accounting for multiplier * base delay * max sample rate
**How to avoid:** Calculate max: 150ms * 33 * 96000Hz = 475,200 samples. Round up to next power of 2 is unnecessary (DelayLine handles non-power-of-2). Call `setMaximumDelayInSamples(475200)` in prepareToPlay, never on audio thread.
**Warning signs:** Distortion or silence at high multiplier values at 96kHz

### Pitfall 2: Read Pointer Corruption in Multi-Tap
**What goes wrong:** Setting updateReadPointer=true on each popSample call advances the read position, making subsequent taps read from wrong locations
**Why it happens:** The default for updateReadPointer is true
**How to avoid:** Pass `false` for all pops except the last one per sample. Or better: pass `false` for all and manually manage if needed. Actually, the cleanest approach is `false` for all 8 taps -- the read pointer only needs to advance by 1 sample per push, which pushSample already handles.
**Warning signs:** Taps produce unexpected delays, pitch artifacts

### Pitfall 3: Zipper Noise from Unsmoothed Parameters
**What goes wrong:** Audible stepping/clicking when automating delay time, level, or mix parameters
**Why it happens:** Parameter changes applied at block rate (~100Hz at 512 samples/44.1kHz) instead of sample rate
**How to avoid:** Smooth every parameter per-sample using OnePoleSmooth. Use 7-15ms for delay/filter params, 5ms for gain crossfades (per CLAUDE.md).
**Warning signs:** Audible ticking or buzzing when automating parameters in DAW

### Pitfall 4: Denormal Numbers in Feedback Path
**What goes wrong:** CPU spikes when delay tails decay to near-zero
**Why it happens:** Denormalized floats (very small non-zero values) trigger slow FPU paths
**How to avoid:** `juce::ScopedNoDenormals` at top of processBlock (already in place). Also consider adding a tiny DC offset or using `snapToZero()` on filters.
**Warning signs:** CPU usage remains high long after audio stops

### Pitfall 5: prepareToPlay Called Multiple Times
**What goes wrong:** State is lost or resources leak when DAW changes sample rate or buffer size
**Why it happens:** prepareToPlay can be called multiple times during plugin lifetime
**How to avoid:** Re-initialize all DSP objects cleanly. Do not assume it is called only once. Reset smoothers, recalculate alpha values, resize delay lines.
**Warning signs:** Glitches after changing sample rate in DAW preferences

### Pitfall 6: Character Filter Cutoff Modulation Clicks
**What goes wrong:** Changing the Character knob produces clicks from abrupt filter coefficient changes
**Why it happens:** IIR filters (not TPT) can produce transients when coefficients change
**How to avoid:** Use FirstOrderTPTFilter which is modulation-safe by design. Still smooth the cutoff frequency parameter to avoid rapid jumps.
**Warning signs:** Clicking when sweeping Character knob

### Pitfall 7: Tap Position Smoothing Causes Pitch Artifacts
**What goes wrong:** Smoothing the delay time of a tap causes Doppler-like pitch shifting during transitions
**Why it happens:** Changing read position in a delay line is equivalent to speeding up or slowing down playback
**How to avoid:** This is actually desirable for small changes (natural feel). For large jumps (e.g., loading a tap preset), consider crossfading between old and new delay time over ~50ms rather than smoothing through all intermediate values. Alternatively, accept the pitch sweep as a feature -- many delay plugins do.
**Warning signs:** Audible pitch bend when recalling tap presets

## Code Examples

### Parameter Layout Definition
```cpp
// Source: JUCE 8 APVTS documentation + CLAUDE.md conventions
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Base delay time: 10-150ms, skewed toward lower values
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"BASE_DELAY", 1}, "Base Delay",
        juce::NormalisableRange<float>(10.0f, 150.0f, 0.1f, 0.5f), // skew 0.5
        80.0f, "ms"));

    // Multiplier: 1x to 33x (max total = 150*33 = 4950ms ~5s)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MULTIPLIER", 1}, "Multiplier",
        juce::NormalisableRange<float>(1.0f, 33.0f, 0.01f, 0.4f), // skew toward lower values
        1.0f, "x"));

    // Wet/dry mix: 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MIX", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f, "%"));

    // Character: 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"CHARACTER", 1}, "Character",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        25.0f, "%"));

    // Quantize toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"QUANTIZE", 1}, "Quantize", false));

    // Per-tap parameters (8 taps)
    for (int i = 1; i <= 8; ++i)
    {
        auto id = juce::String(i);

        // Tap position: 0.0-1.0 ratio along delay line
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + id + "_POS", 1},
            "Tap " + id + " Position",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            static_cast<float>(i) / 8.0f)); // Default: equal spacing

        // Tap level: 0.0-1.0 linear gain
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + id + "_LEVEL", 1},
            "Tap " + id + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            1.0f)); // Default: full level
    }

    return layout;
}
```

### processBlock Orchestration
```cpp
// Source: Architecture pattern derived from JUCE DelayLine API + CLAUDE.md rules
void ZeitraumProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // Push dry samples into DryWetMixer before processing
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Load parameter targets (atomic, lock-free)
    float baseDelayTarget = baseDelayParam->load();
    float multiplierTarget = multiplierParam->load();
    float characterTarget = characterParam->load() / 100.0f;
    bool quantize = quantizeParam->load() > 0.5f;

    // Update smoother targets
    baseDelaySmoother.setTargetValue(baseDelayTarget);
    multiplierSmoother.setTargetValue(multiplierTarget);
    characterSmoother.setTargetValue(characterTarget);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float baseDelayMs = baseDelaySmoother.getNextValue();
            float mult = multiplierSmoother.getNextValue();
            float character = characterSmoother.getNextValue();

            // Apply character to input before pushing to delay line
            float processed = characterProcessor.process(ch, channelData[i], character);
            delayLine[ch].pushSample(0, processed);

            // Sum taps
            float wetSample = 0.0f;
            for (int tap = 0; tap < 8; ++tap)
            {
                float pos = tapPositionSmoothers[tap].getNextValue();
                float level = tapLevelSmoothers[tap].getNextValue();
                float delayMs = pos * baseDelayMs * mult;
                if (quantize)
                    delayMs = std::round(delayMs / 10.0f) * 10.0f;
                delayMs = std::max(delayMs, 0.01f);
                float delaySamples = delayMs * 0.001f * static_cast<float>(currentSampleRate);
                float tapOut = delayLine[ch].popSample(0, delaySamples, false);
                wetSample += tapOut * level;
            }
            channelData[i] = wetSample;
        }
    }

    // Mix wet with dry
    dryWetMixer.mixWetSamples(block);

    // Clear extra channels
    for (int ch = numChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}
```

### Tap Preset Save/Recall via ValueTree
```cpp
// Source: JUCE ValueTree documentation
// Save current tap positions as a named preset within the plugin state
void saveTapPreset(const juce::String& name)
{
    juce::ValueTree preset("TapPreset");
    preset.setProperty("name", name, nullptr);
    for (int i = 1; i <= 8; ++i)
    {
        auto paramId = "TAP" + juce::String(i) + "_POS";
        preset.setProperty(paramId, apvts.getRawParameterValue(paramId)->load(), nullptr);
    }
    // Store in APVTS state tree under a "TapPresets" child
    auto presets = apvts.state.getOrCreateChildWithName("TapPresets", nullptr);
    presets.addChild(preset, -1, nullptr);
}

void recallTapPreset(const juce::String& name)
{
    auto presets = apvts.state.getChildWithName("TapPresets");
    for (int i = 0; i < presets.getNumChildren(); ++i)
    {
        auto preset = presets.getChild(i);
        if (preset.getProperty("name").toString() == name)
        {
            for (int t = 1; t <= 8; ++t)
            {
                auto paramId = "TAP" + juce::String(t) + "_POS";
                if (auto* param = apvts.getParameter(paramId))
                    param->setValueNotifyingHost(
                        param->convertTo0to1(preset.getProperty(paramId)));
            }
            break;
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual circular buffer | juce::dsp::DelayLine | JUCE 5.4+ | Built-in interpolation modes, multi-tap support via popSample |
| Direct-form IIR filters | TPT (Topology-Preserving Transform) filters | JUCE 5.4+ | Modulation-safe, no coefficient-change artifacts |
| Block-rate parameter updates | Per-sample smoothing | Always best practice | Eliminates zipper noise at all buffer sizes |
| juce::SmoothedValue (linear ramp) | One-pole exponential smoother | Project convention | More natural response curve, matches CLAUDE.md spec |

**Deprecated/outdated:**
- `juce::dsp::StateVariableFilter` (old namespace): Use `juce::dsp::StateVariableTPTFilter` or `FirstOrderTPTFilter` instead
- Manual `AudioBuffer` ring buffer management: `DelayLine` handles all wrapping and interpolation

## Open Questions

1. **DelayLine channel count for multi-tap popSample**
   - What we know: DelayLine is initialized with ProcessSpec which includes numChannels. When using dual-mono (separate DelayLine per channel), each line only needs 1 channel.
   - What's unclear: Whether popSample with channel=0 works correctly when the DelayLine was prepared with numChannels=1
   - Recommendation: Initialize each DelayLine with ProcessSpec{sampleRate, blockSize, 1} and always use channel=0. Verified from the header that writePos/readPos are per-channel vectors, so single-channel usage is valid.

2. **Tap preset mechanism scope**
   - What we know: CORE-08 requires save/recall of tap presets. The CONTEXT.md lists it under Claude's discretion.
   - What's unclear: Whether presets should persist across DAW sessions (stored in plugin state XML) or be global (stored on disk)
   - Recommendation: Store within plugin state XML for now (survives save/load with DAW session). A global preset system can be added later if needed. The default equal-spacing preset should be hardcoded, not stored.

3. **popSample with updateReadPointer=false for all taps**
   - What we know: The DelayLine documentation says use updateReadPointer for multi-tap. Setting it to false means the read pointer does not advance.
   - What's unclear: If we never update the read pointer, does the internal state remain consistent? pushSample advances the write pointer. The read pointer is only needed for the default popSample (delayInSamples = -1).
   - Recommendation: Pass false for all 8 taps. The read pointer is only used when popSample is called with delayInSamples = -1 (the default). Since we always pass an explicit delay value, the read pointer state is irrelevant. Verified from the source code: when delayInSamples >= 0, interpolation reads directly relative to writePos, not readPos.

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
| CORE-01 | 8 taps produce output at correct positions | unit | `make test` | No -- Wave 0 |
| CORE-02 | Base delay time produces correct delay in samples | unit | `make test` | No -- Wave 0 |
| CORE-03 | Multiplier scales all tap times proportionally | unit | `make test` | No -- Wave 0 |
| CORE-04 | Per-tap level attenuates correctly | unit | `make test` | No -- Wave 0 |
| CORE-05 | Free tap positioning reads at correct delay | unit | `make test` | No -- Wave 0 |
| CORE-06 | Default equal spacing preset is correct | unit | `make test` | No -- Wave 0 |
| CORE-07 | Quantization snaps to 10ms grid | unit | `make test` | No -- Wave 0 |
| CORE-08 | Tap presets save and restore | integration | `make test` | No -- Wave 0 |
| CORE-09 | Wet/dry mix blends correctly | integration | `make test` | No -- Wave 0 |
| MIX-01 | Stereo input/output operates correctly | integration | `make test` | Partial (bus layout test exists) |
| INTG-04 | Character produces HF attenuation | unit | `make test` | No -- Wave 0 |
| GUI-04 | Parameter changes produce no clicks | manual-only | Listen test in DAW | N/A |
| INFR-04 | Glitch-free at 64 samples/44.1kHz and 96kHz | manual-only | Run in DAW at various buffer sizes | N/A |

### Sampling Rate
- **Per task commit:** `make test`
- **Per wave merge:** `make test && make validate`
- **Phase gate:** Full suite green + manual listening test before verify

### Wave 0 Gaps
- [ ] `test/dsp/OnePoleSmoothTests.cpp` -- smoother convergence and time constant
- [ ] `test/dsp/TapReaderTests.cpp` -- position math, quantization
- [ ] `test/dsp/CharacterProcessorTests.cpp` -- HF attenuation measurement
- [ ] `test/dsp/DelayEngineTests.cpp` -- multi-tap output, delay accuracy
- [ ] Update `CMakeLists.txt` -- add new test source files to ZeitraumTests target

## Sources

### Primary (HIGH confidence)
- JUCE 8.0.12 source code at `lib/JUCE/modules/juce_dsp/` -- DelayLine, FirstOrderTPTFilter, DryWetMixer, IIR::Filter headers read directly
- CLAUDE.md project conventions -- parameter access, smoothing formula, audio thread rules, source layout

### Secondary (MEDIUM confidence)
- JUCE DelayLine popSample multi-tap pattern -- verified from source that updateReadPointer=false prevents read pointer advancement, explicit delayInSamples bypasses read pointer entirely

### Tertiary (LOW confidence)
- BBD character tuning values (4kHz cutoff, noise level) -- these are starting points that will need ear-tuning; functional correctness can be tested but sonic quality is subjective

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components verified from JUCE 8.0.12 source headers in the project submodule
- Architecture: HIGH -- multi-tap popSample pattern verified from DelayLine source; dual-mono pattern standard for stereo delays
- Pitfalls: HIGH -- based on verified JUCE API behavior (updateReadPointer default, prepareToPlay lifecycle) and established audio DSP knowledge
- Character tuning: MEDIUM -- filter topology and noise approach are sound, but specific parameter values need listening tests

**Research date:** 2026-03-05
**Valid until:** 2026-04-05 (stable -- JUCE 8 API unlikely to change)
