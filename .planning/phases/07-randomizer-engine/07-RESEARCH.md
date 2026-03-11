# Phase 7: Randomizer Engine - Research

**Researched:** 2026-03-10
**Domain:** JUCE parameter manipulation, constrained random generation, GUI integration
**Confidence:** HIGH

## Summary

The randomizer engine needs to generate musically useful random values for all sound-shaping parameters (tap positions, tap levels, feedback routing, feedback filters, multiplier, wet/dry mix) while excluding mode/trigger parameters and enforcing safety constraints. The primary technical challenges are: (1) calling `setValueNotifyingHost` on the message thread (not the audio thread), (2) sorting tap positions ascending after randomization, and (3) bounding feedback gains to prevent instability.

The codebase already has a precedent for programmatic parameter setting: the `OUTPUT_MIX` preset system in `processBlock` calls `setValueNotifyingHost` on tap level parameters. However, STATE.md notes that "setValueNotifyingHost must be called on message thread, not audio thread" -- the existing OUTPUT_MIX code actually violates this rule. The randomizer should be triggered from the GUI button callback (message thread), which is the correct approach. The APVTS constructor currently passes `nullptr` for UndoManager, so undo-transaction wrapping is not available without adding one.

**Primary recommendation:** Implement randomization as a method on ZeitraumProcessor called from the GUI button's onClick callback (message thread). Use `juce::Random` for generation, apply per-parameter range constraints, sort tap positions ascending, and normalize feedback gains to sum <= 0.8.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RAND-01 | Randomize button generates new random values for all sound-shaping parameters | Architecture pattern: randomize method iterates parameter groups, uses NormalisableRange to generate valid values |
| RAND-02 | Randomized tap positions sorted ascending | Generate 8 random values in [0,1], sort, assign to TAP1_POS..TAP8_POS |
| RAND-03 | Feedback gain sum normalized to ~80% max | Sum all 12 feedback source gains; if sum > 0.8, scale proportionally |
| RAND-04 | Wet/dry clamped to [0.2, 0.9] range | Map random float to [20.0, 90.0] for MIX parameter (0-100% range) |
| RAND-05 | Mode/trigger parameters excluded | Exclude list: OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, NOTE_DIV, RANDOMIZE |
| GUI-01 | Randomize button visible in plugin editor UI | Add TextButton to TopBar or editor, wire onClick to processor randomize method |
</phase_requirements>

## Standard Stack

### Core

No new libraries needed. Everything uses existing JUCE infrastructure.

| Component | Source | Purpose | Why Standard |
|-----------|--------|---------|--------------|
| `juce::Random` | juce_core | Thread-safe PRNG | Built into JUCE, seedable, fast |
| `juce::RangedAudioParameter::setValueNotifyingHost` | juce_audio_processors | Set parameter values programmatically | Updates APVTS, notifies GUI attachments, records automation |
| `std::sort` | C++ stdlib | Sort tap positions ascending | Standard algorithm |

### Supporting

| Component | Source | Purpose | When to Use |
|-----------|--------|---------|-------------|
| `juce::NormalisableRange` | juce_core | Convert between normalized [0,1] and actual parameter ranges | When generating random values within parameter bounds |
| `juce::TextButton` | juce_gui_basics | Randomize button widget | GUI-01 requirement |

## Architecture Patterns

### Pattern 1: Processor-Side Randomize Method

**What:** A public method on `ZeitraumProcessor` that generates and applies random values to all sound-shaping parameters.

**When to use:** Called from GUI button click (message thread).

**Why this pattern:** Keeps randomization logic centralized in the processor where parameter knowledge lives. The GUI just calls the method. Using `setValueNotifyingHost` ensures: (a) APVTS state updates, (b) GUI attachments (sliders, bars, cells) update automatically, (c) DAW automation is notified, (d) state persistence works via existing `getStateInformation`.

**Example:**
```cpp
// In ZeitraumProcessor.h:
void randomizeParameters();

// In ZeitraumProcessor.cpp:
void ZeitraumProcessor::randomizeParameters()
{
    juce::Random rng;  // Uses system seed

    // 1. Generate and sort tap positions
    float positions[8];
    for (int i = 0; i < 8; ++i)
        positions[i] = rng.nextFloat();
    std::sort(std::begin(positions), std::end(positions));

    for (int i = 0; i < 8; ++i)
    {
        auto* param = apvts.getParameter("TAP" + juce::String(i + 1) + "_POS");
        param->setValueNotifyingHost(param->convertTo0to1(positions[i]));
    }

    // 2. Random tap levels [0, 1]
    for (int i = 0; i < 8; ++i)
    {
        auto* param = apvts.getParameter("TAP" + juce::String(i + 1) + "_LEVEL");
        param->setValueNotifyingHost(rng.nextFloat());
    }

    // 3. Random feedback gains, normalized to sum <= 0.8
    float fbGains[12];
    float sum = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        fbGains[i] = rng.nextFloat();
        sum += fbGains[i];
    }
    float scale = (sum > 0.0f) ? (0.8f / sum) : 0.0f;
    // ... apply scaled gains via setValueNotifyingHost

    // 4. Mix clamped to [20, 90]
    float mix = 20.0f + rng.nextFloat() * 70.0f;
    auto* mixP = apvts.getParameter("MIX");
    mixP->setValueNotifyingHost(mixP->convertTo0to1(mix));
}
```

### Pattern 2: Parameter Exclusion List

**What:** Explicit list of parameter IDs that must NOT be randomized.

**Why:** RAND-05 requires OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, NOTE_DIV remain unchanged. Future Phase 8 adds RANDOMIZE parameter which must also be excluded.

**Excluded parameters:**
- `OUTPUT_MIX` -- mode selector, not a sound-shaping parameter
- `TEMPO_SYNC` -- mode toggle
- `QUANTIZE` -- mode toggle
- `NOTE_DIV` -- mode selector
- `RANDOMIZE` -- trigger parameter (Phase 8, but exclude now to be forward-compatible)

### Pattern 3: Feedback Gain Normalization

**What:** After generating random feedback gains for all 12 sources (8 individual taps + 4 preset mixes), scale them so the total sum does not exceed ~0.8.

**Why:** The FeedbackSaturator has a tanh soft clip and RMS limiter (threshold 0.85), but these are reactive safety nets. Pre-constraining gains prevents transient instability in the first few hundred milliseconds before the limiter reacts.

**Algorithm:**
1. Generate 12 random values in [0, 1]
2. Compute sum
3. If sum > 0.8, multiply each by (0.8 / sum)
4. Convert each to the parameter range [0, 100] (FB_TAP and FB_MIX params use 0-100% range)

**Edge case:** If all random values happen to be 0 (astronomically unlikely), skip normalization.

### Anti-Patterns to Avoid

- **Randomizing on the audio thread:** Never call `setValueNotifyingHost` from `processBlock`. The existing OUTPUT_MIX preset code does this and should ideally be refactored, but the randomizer must not repeat this pattern. Always trigger from message thread (GUI callback).
- **Using `getRawParameterValue` for writing:** Raw pointers are read-only in the APVTS contract. Use `getParameter()->setValueNotifyingHost()` to write.
- **Randomizing in normalized [0,1] space without understanding the range:** Parameters like BASE_DELAY have skewed ranges (0.5 skew factor). Randomizing the normalized value directly would bias toward high delay values. Generate in actual-value space, then convert to normalized.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Random number generation | Custom PRNG | `juce::Random` | Thread-safe, well-seeded, standard in JUCE ecosystem |
| Parameter range conversion | Manual min/max math | `param->convertTo0to1(actualValue)` | Handles skewed ranges (BASE_DELAY, MULTIPLIER have non-linear skew) |
| GUI-parameter sync | Manual slider updates | `setValueNotifyingHost` | Automatically updates all attached GUI components via APVTS listener system |

**Key insight:** `setValueNotifyingHost` does all the heavy lifting -- it updates the parameter tree, notifies the host for automation, and triggers GUI attachment updates. No manual GUI refresh needed.

## Common Pitfalls

### Pitfall 1: Thread Safety of setValueNotifyingHost

**What goes wrong:** Calling `setValueNotifyingHost` from the audio thread can cause deadlocks or race conditions in some DAW hosts.
**Why it happens:** The method may trigger host callbacks that acquire locks.
**How to avoid:** Only call from the message thread. The GUI button onClick runs on the message thread, so this is safe. STATE.md already flags this as a known concern.
**Warning signs:** Intermittent hangs when randomizing during playback.

### Pitfall 2: Skewed Parameter Ranges

**What goes wrong:** Generating a uniform random number in [0,1] and passing it directly as the normalized value produces musically biased results for skewed parameters.
**Why it happens:** BASE_DELAY has skew 0.5 (square root mapping), MULTIPLIER has skew 0.4. A uniform random in normalized space clusters values at the high end of the actual range.
**How to avoid:** Generate random values in the actual parameter range (e.g., 10-150 for BASE_DELAY), then use `convertTo0to1()` to get the normalized value.
**Warning signs:** Randomized delays are almost always long; randomized multipliers are almost always high.

### Pitfall 3: Feedback Filter Frequency Ordering

**What goes wrong:** HP frequency randomized above LP frequency creates a bandpass with no passband.
**Why it happens:** FB_HP_FREQ range is 20-2000 Hz, FB_LP_FREQ range is 200-20000 Hz. Overlap zone is 200-2000 Hz.
**How to avoid:** After randomizing both frequencies, ensure HP < LP. If HP >= LP, swap them or clamp HP below LP.
**Warning signs:** Feedback signal is completely silent after randomization.

### Pitfall 4: Quantize Interaction with Randomized Tap Positions

**What goes wrong:** If QUANTIZE is enabled, randomized tap positions snap to 10ms grid, potentially causing multiple taps to land on the same position.
**Why it happens:** Quantization reduces the effective resolution of tap positions.
**How to avoid:** This is a user-controlled mode parameter that we don't randomize (RAND-05), so the behavior is expected. No special handling needed -- if the user has quantize on, snapping is intentional.
**Warning signs:** Multiple taps at same time position (acceptable behavior).

### Pitfall 5: Boolean Parameters (FB_HP_ON, FB_LP_ON)

**What goes wrong:** Forgetting to randomize the filter enable toggles, leaving filters always off or always on.
**Why it happens:** These are AudioParameterBool, not AudioParameterFloat, easy to miss.
**How to avoid:** Include FB_HP_ON and FB_LP_ON in the randomization set. Use `rng.nextBool()` or `rng.nextFloat() > 0.5f`.

## Code Examples

### Complete Parameter Inventory for Randomization

Parameters TO randomize (sound-shaping):
```
// Per-tap (8 each = 16 total)
TAP1_POS .. TAP8_POS     // float [0, 1] -- MUST sort ascending after
TAP1_LEVEL .. TAP8_LEVEL  // float [0, 1]

// Feedback routing (12 total)
FB_TAP1 .. FB_TAP8        // float [0, 100] %
FB_ODD, FB_EVEN           // float [0, 100] %
FB_RISING, FB_FALLING     // float [0, 100] %

// Feedback filters (4 total)
FB_HP_FREQ                // float [20, 2000] Hz, skew 0.3
FB_LP_FREQ                // float [200, 20000] Hz, skew 0.3
FB_HP_ON                  // bool
FB_LP_ON                  // bool

// Global sound-shaping (3 total)
BASE_DELAY                // float [10, 150] ms, skew 0.5
MULTIPLIER                // float [1, 33] x, skew 0.4
MIX                       // float [0, 100] % -- clamp to [20, 90]
CHARACTER                 // float [0, 100] %
```

Parameters to EXCLUDE (mode/trigger):
```
OUTPUT_MIX    // choice -- mode selector
TEMPO_SYNC    // bool -- mode toggle
QUANTIZE      // bool -- mode toggle
NOTE_DIV      // choice -- mode selector
RANDOMIZE     // bool -- trigger (Phase 8, future-proof exclusion)
```

Total randomized: ~35 parameters
Total excluded: 5 parameters

### Setting a Parameter Value from Message Thread

```cpp
// Correct pattern: get the RangedAudioParameter, convert actual to normalized, notify host
auto* param = apvts.getParameter("BASE_DELAY");
float actualValue = 10.0f + rng.nextFloat() * 140.0f;  // [10, 150]
param->setValueNotifyingHost(param->convertTo0to1(actualValue));
```

### GUI Button Integration

```cpp
// In TopBar constructor or ZeitraumEditor:
randomizeButton.setButtonText("Randomize");
randomizeButton.onClick = [this]()
{
    processorRef.randomizeParameters();
};
addAndMakeVisible(randomizeButton);
```

## State of the Art

| Aspect | Current State | Impact |
|--------|---------------|--------|
| OUTPUT_MIX preset apply | Called from processBlock (audio thread) | Known thread safety issue; randomizer should NOT follow this pattern |
| APVTS UndoManager | Constructor passes `nullptr` | No undo transaction wrapping available; adding UndoManager is optional but not needed for v1.2 |
| Feedback safety | FeedbackSaturator has tanh + RMS limiter at 0.85 threshold | Provides a safety net, but randomizer should still pre-constrain gains to ~0.8 sum |

## Open Questions

1. **Button placement in GUI**
   - What we know: TopBar uses FlexBox layout with sliders, toggles, and combos. There is space to add one more item.
   - What's unclear: Whether the randomize button belongs in TopBar or elsewhere (e.g., near the preset selector in the right panel).
   - Recommendation: Add to TopBar for maximum visibility. It's a global action, not tap-specific. The planner can decide exact placement.

2. **CHARACTER parameter range for randomization**
   - What we know: CHARACTER is 0-100%, currently defaults to 25%. High values add significant noise and HF roll-off.
   - What's unclear: Whether full 0-100% range produces musically useful results at extremes.
   - Recommendation: Randomize full range. The user can re-randomize if they get an extreme value. No clamping specified in requirements.

## Sources

### Primary (HIGH confidence)
- Codebase inspection: `src/PluginProcessor.cpp` -- full parameter layout, ranges, skew factors
- Codebase inspection: `src/PluginProcessor.h` -- cached parameter pointers, APVTS setup
- Codebase inspection: `src/dsp/FeedbackMatrix.h` -- 12-source feedback architecture
- Codebase inspection: `src/dsp/FeedbackSaturator.h` -- tanh + RMS limiter, threshold 0.85
- Codebase inspection: `src/ui/TopBar.h` -- GUI layout, attachment pattern
- Codebase inspection: `src/PluginEditor.cpp` -- editor layout, component hierarchy
- Project state: `.planning/STATE.md` -- setValueNotifyingHost thread safety decision
- Requirements: `.planning/REQUIREMENTS.md` -- RAND-01 through RAND-05, GUI-01

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all components are existing JUCE APIs already used in the codebase
- Architecture: HIGH - pattern follows existing OUTPUT_MIX preset approach, corrected for thread safety
- Pitfalls: HIGH - identified from codebase inspection and STATE.md documented concerns

**Research date:** 2026-03-10
**Valid until:** 2026-04-10 (stable domain, no external dependencies)
