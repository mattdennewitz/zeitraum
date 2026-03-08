# Phase 4: DAW Integration - Research

**Researched:** 2026-03-07
**Domain:** JUCE plugin parameter organization, tempo sync, state persistence
**Confidence:** HIGH

## Summary

Phase 4 focuses on three integration concerns: (1) organizing the existing flat parameter layout into grouped AudioProcessorParameterGroup hierarchy for better DAW automation UX, (2) adding tempo sync via host BPM reading from the playhead, and (3) ensuring state persistence handles the new parameters with backward compatibility. All three concerns operate within well-established JUCE patterns that the project already uses extensively.

The key insight is that tempo sync requires minimal DSP changes -- the conversion from note division to milliseconds happens in the processor before passing `baseDelayMs` to DelayEngine. The existing OnePoleSmooth architecture handles tempo change crossfades naturally. Parameter grouping is purely a layout refactor with no behavioral change. State persistence follows the existing versioned XML pattern.

**Primary recommendation:** Implement parameter grouping first (purely structural, easy to verify), then tempo sync (new parameters + processBlock logic), then state persistence tests last (builds on both).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Tempo sync replaces the base delay time with a musical note division (e.g. 1/8 note at 120 BPM = 250ms)
- Multiplier knob stays active in sync mode -- scales the synced base time for creative range beyond standard note values
- Tap positions continue to work as ratios of the (synced) base delay, same as free-running mode
- Minimal note division set: 1/4, 1/8, dotted 1/8, triplet 1/8 (~6 options)
- When DAW tempo changes mid-playback, delay time smoothly crossfades to new value (consistent with existing OnePoleSmooth architecture)
- New APVTS parameters: TEMPO_SYNC (bool toggle), NOTE_DIV (choice parameter)
- All parameters automatable -- no exceptions, including OUTPUT_MIX and QUANTIZE
- Parameters organized using AudioProcessorParameterGroup: "Tap 1 > Position", "Tap 1 > Level", "Feedback > Tap 1", "Feedback > HP Freq", etc.
- Refactor flat parameter layout into grouped layout
- Parameter display shows formatted values with units: "80.0 ms", "50%", "440 Hz", "1/8 note"
- Improve valueToText lambdas where current formatting is insufficient
- All new parameters (TEMPO_SYNC, NOTE_DIV) saved/restored via APVTS automatically
- Silent defaults for missing parameters when loading older sessions -- no user notification
- Bump pluginVersion from 2 to 3 in getStateInformation
- No migration code needed -- APVTS handles missing params gracefully with defaults

### Claude's Discretion
- Exact parameter group hierarchy and naming
- How to handle host BPM not available (fallback to 120 BPM or disable sync)
- Smoother time constant for tempo changes
- Whether to add 1/16 and 1/2 note divisions beyond the minimal set

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INTG-01 | All parameters are DAW-automatable | Parameter grouping via AudioProcessorParameterGroup gives DAW automation lanes organized display; all params already automatable via APVTS, grouping improves discoverability |
| INTG-02 | Tempo sync available with host BPM and note divisions | PlayHead::PositionInfo::getBpm() provides host BPM; note division converts to ms before passing to DelayEngine; OnePoleSmooth handles crossfades |
| INTG-03 | Plugin state saves and restores with DAW session | Existing XML state mechanism with pluginVersion attribute; APVTS handles missing params with defaults; bump version to 3 |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Framework (already in project) | Audio plugin standard |
| juce::AudioProcessorParameterGroup | 8.0.12 | Parameter organization for DAW display | JUCE's built-in mechanism for grouping parameters in automation lanes |
| juce::AudioPlayHead::PositionInfo | 8.0.12 | Host tempo/BPM reading | JUCE 8 modern API (replaces deprecated CurrentPositionInfo) |
| juce::AudioParameterChoice | 8.0.12 | Note division selector | Already used for OUTPUT_MIX; correct type for discrete choices |
| juce::AudioParameterBool | 8.0.12 | Tempo sync toggle | Already used for QUANTIZE, FB_HP_ON, etc. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| OnePoleSmooth (existing) | N/A | Tempo change crossfade | When BPM changes, the effective delay time smoothly transitions |
| Catch2 | 3.7.1 | Testing | Verify parameter existence, state round-trip, tempo calculations |

### Alternatives Considered
None -- all components are existing JUCE APIs already used in the project.

## Architecture Patterns

### Parameter Group Hierarchy (Recommended)

```
Global
  |- Base Delay
  |- Multiplier
  |- Mix
  |- Character
  |- Quantize
  |- Tempo Sync
  |- Note Division
Tap 1
  |- Position
  |- Level
Tap 2
  |- Position
  |- Level
...
Tap 8
  |- Position
  |- Level
Feedback
  |- Tap 1
  |- Tap 2
  ...
  |- Tap 8
  |- Odd
  |- Even
  |- Rising
  |- Falling
  |- HP Freq
  |- LP Freq
  |- HP On
  |- LP On
Output
  |- Output Mix
```

This hierarchy maps directly to DAW automation lane organization. VST3 and AUv3 support nested subgroups. AU (v2) flattens groups with the separator string (typically " | ").

### Pattern 1: AudioProcessorParameterGroup in ParameterLayout

**What:** Replace flat `layout.add()` calls with grouped additions
**When to use:** Always for organized DAW automation

```cpp
// Source: JUCE 8.0.12 AudioProcessorValueTreeState.h
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global parameters group
    auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "global", "Global", " | ");
    globalGroup->addChild(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"BASE_DELAY", 1}, "Base Delay",
        juce::NormalisableRange<float>(10.0f, 150.0f, 0.1f, 0.5f),
        80.0f, "ms"));
    // ... more global params
    layout.add(std::move(globalGroup));

    // Per-tap groups
    for (int i = 1; i <= 8; ++i)
    {
        auto tapGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "tap" + juce::String(i), "Tap " + juce::String(i), " | ");
        tapGroup->addChild(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + juce::String(i) + "_POS", 1},
            "Position",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            static_cast<float>(i) / 8.0f));
        tapGroup->addChild(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + juce::String(i) + "_LEVEL", 1},
            "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            1.0f));
        layout.add(std::move(tapGroup));
    }

    return layout;
}
```

**Key point:** Parameter IDs (e.g., "BASE_DELAY", "TAP1_POS") remain identical. Only the grouping container changes. All `getRawParameterValue()` calls in the constructor continue to work unchanged.

### Pattern 2: Tempo Sync in processBlock

**What:** Read host BPM, convert note division to ms, pass as base delay
**When to use:** When TEMPO_SYNC is enabled

```cpp
// In processBlock:
float effectiveBaseDelay = baseDelayParam->load();

if (tempoSyncParam->load() > 0.5f)
{
    double bpm = 120.0; // fallback
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
        }
    }

    // Convert note division to ms
    int divIndex = static_cast<int>(noteDivParam->load());
    double beatMs = 60000.0 / bpm;  // ms per quarter note

    // Note division multipliers relative to quarter note
    static constexpr double divMultipliers[] = {
        1.0,    // 1/4
        0.5,    // 1/8
        0.75,   // dotted 1/8 (1/8 + 1/16)
        1.0/3.0, // triplet 1/8 (1/4 / 3)
        0.25,   // 1/16
        2.0     // 1/2
    };

    effectiveBaseDelay = static_cast<float>(beatMs * divMultipliers[divIndex]);

    // Clamp to delay line maximum
    effectiveBaseDelay = std::min(effectiveBaseDelay, 150.0f);
}
```

**Important:** The effective base delay is passed to `delayEngine.process()` exactly where `baseDelayParam->load()` was used before. The multiplier and tap positions still work on top of this value. OnePoleSmooth inside DelayEngine handles the transition automatically.

### Pattern 3: PlayHead API Usage (JUCE 8 Modern)

**What:** Use `PositionInfo` with `Optional` returns
**When to use:** Always in JUCE 8 -- the old `getCurrentPosition` is deprecated

```cpp
// Source: JUCE 8.0.12 juce_AudioPlayHead.h
// PositionInfo::getBpm() returns Optional<double>
// Must handle nullopt (host doesn't provide BPM)

if (auto* playHead = getPlayHead())
{
    if (auto pos = playHead->getPosition())
    {
        if (auto bpm = pos->getBpm())
        {
            // Use *bpm
        }
    }
}
```

**The triple-check pattern** (playHead != nullptr, position has value, bpm has value) is necessary because:
1. `getPlayHead()` returns nullptr in standalone or during construction
2. `getPosition()` returns nullopt when host provides no timing
3. `getBpm()` returns nullopt when host doesn't provide tempo (rare but possible)

### Pattern 4: valueToText Formatting

**What:** Custom display strings for parameter values in DAW
**When to use:** For all parameters that need formatted display

```cpp
// AudioParameterFloat with custom valueToText/textToValue
auto param = std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"BASE_DELAY", 1}, "Base Delay",
    juce::NormalisableRange<float>(10.0f, 150.0f, 0.1f, 0.5f),
    80.0f,
    juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int) {
            return juce::String(value, 1) + " ms";
        })
        .withValueFromStringFunction([](const juce::String& text) {
            return text.getFloatValue();
        }));
```

**Note:** In JUCE 8, the `AudioParameterFloat` constructor that takes a simple label string as the last parameter (e.g., `"ms"`) will display the label next to the value. For more control, use `AudioParameterFloatAttributes` with explicit lambdas. The current codebase uses the simple label string approach which is acceptable for most parameters.

### Anti-Patterns to Avoid
- **Caching playhead BPM across blocks:** BPM can change between any two processBlock calls. Always re-read each block.
- **Modifying delay line max size for tempo sync:** The existing max of 150ms * 33x = ~5s is more than sufficient for any musical note division at reasonable BPM. No need to resize.
- **Rounding note division ms to integer:** Pass the exact floating-point ms value to the smoother. Rounding introduces tempo drift.
- **Using deprecated getCurrentPosition:** Use the modern `getPosition()` which returns `Optional<PositionInfo>`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter grouping | Custom parameter tree | AudioProcessorParameterGroup | JUCE handles VST3/AU/AUv3 group display automatically |
| BPM reading | Manual host communication | getPlayHead()->getPosition()->getBpm() | Standard JUCE API, tested across all hosts |
| Parameter smoothing on tempo change | New smoother for tempo | Existing OnePoleSmooth via baseDelaySmoother in DelayEngine | Delay time already smoothed; feeding new target values is sufficient |
| State versioning | Custom serialization | APVTS replaceState + pluginVersion attribute | Already established pattern in the project |

**Key insight:** Tempo sync is primarily a parameter conversion (note division + BPM -> ms). It feeds into the existing delay time path with zero DSP changes needed.

## Common Pitfalls

### Pitfall 1: Parameter ID Changes Break State
**What goes wrong:** Changing parameter IDs when refactoring to groups causes old sessions to lose their settings.
**Why it happens:** APVTS identifies parameters by ID string, not by group hierarchy.
**How to avoid:** Keep ALL existing parameter IDs identical (BASE_DELAY, TAP1_POS, etc.). Only change the grouping container. The group ID is separate from the parameter ID.
**Warning signs:** State round-trip tests fail after refactoring.

### Pitfall 2: Host BPM Not Available
**What goes wrong:** Plugin crashes or produces wrong delay times when getPlayHead() returns nullptr or getBpm() returns nullopt.
**Why it happens:** In standalone mode, during plugin scan, or in hosts that don't report BPM.
**How to avoid:** Always provide a fallback BPM (120.0 is standard). Use the triple-check pattern for playhead access.
**Warning signs:** Crashes during AU validation (auval runs without a real host playhead).

### Pitfall 3: Tempo Sync Delay Exceeds Buffer
**What goes wrong:** At very low BPM (e.g., 40 BPM), a 1/2 note = 3000ms. With multiplier = 33x, this exceeds the delay line size.
**Why it happens:** Note division ms * multiplier can exceed maxBaseDelayMs * maxMultiplier.
**How to avoid:** Clamp the effective base delay to 150.0f before passing to DelayEngine (or clamp the final delay time = baseDelay * multiplier * tapPosition to delay line max). The existing architecture already handles this via delay line max samples.
**Warning signs:** Delay line reads past allocated buffer, producing garbage or silence.

### Pitfall 4: AU Subgroup Limitation
**What goes wrong:** AU (v2) plugins don't support nested subgroups, so deep hierarchies get flattened.
**Why it happens:** AudioUnit v2 spec limitation.
**How to avoid:** Use a single level of grouping (Global, Tap 1, Tap 2, ..., Feedback, Output). The separator string (e.g., " | ") joins group names when flattened. This project's planned hierarchy is already flat enough.
**Warning signs:** AU validation warnings about parameter naming.

### Pitfall 5: GenericAudioProcessorEditor and Groups
**What goes wrong:** GenericAudioProcessorEditor may display parameters differently after grouping.
**Why it happens:** GenericAudioProcessorEditor respects parameter groups and may organize them visually.
**How to avoid:** This is actually desirable -- it should organize the generic editor by group. But verify the editor still works after refactoring.
**Warning signs:** Parameters missing or duplicated in the generic editor.

## Code Examples

### Note Division Enum and Conversion

```cpp
// Note divisions with multipliers relative to quarter note (1 beat)
// At 120 BPM: quarter note = 500ms
enum NoteDivision
{
    Quarter = 0,    // 1/4 note    = 1.0 beat
    Eighth,         // 1/8 note    = 0.5 beat
    DottedEighth,   // dotted 1/8  = 0.75 beat
    TripletEighth,  // triplet 1/8 = 0.333... beat
    Sixteenth,      // 1/16 note   = 0.25 beat
    Half,           // 1/2 note    = 2.0 beats
    NumDivisions
};

static constexpr double noteDivMultipliers[] = {
    1.0,          // Quarter
    0.5,          // Eighth
    0.75,         // Dotted Eighth
    1.0 / 3.0,   // Triplet Eighth
    0.25,         // Sixteenth
    2.0           // Half
};

// Division names for AudioParameterChoice
static const juce::StringArray noteDivNames = {
    "1/4", "1/8", "1/8 dot", "1/8 trip", "1/16", "1/2"
};

// Conversion: BPM + division -> milliseconds
inline float noteDivisionToMs(double bpm, int divIndex)
{
    double beatMs = 60000.0 / bpm;
    return static_cast<float>(beatMs * noteDivMultipliers[divIndex]);
}
```

### Recommended Note Division Set

The user requested "~6 options" with 1/4, 1/8, dotted 1/8, triplet 1/8 as the minimum. Adding 1/16 and 1/2 rounds out to 6 practical divisions covering the full range. This avoids the complexity of dotted quarter, triplet quarter, etc. while still being musically useful.

| Division | Multiplier | At 120 BPM | At 80 BPM | At 180 BPM |
|----------|-----------|------------|-----------|------------|
| 1/2      | 2.0       | 1000 ms    | 1500 ms   | 667 ms     |
| 1/4      | 1.0       | 500 ms     | 750 ms    | 333 ms     |
| dotted 1/8 | 0.75    | 375 ms     | 562 ms    | 250 ms     |
| 1/8      | 0.5       | 250 ms     | 375 ms    | 167 ms     |
| triplet 1/8 | 0.333  | 167 ms     | 250 ms    | 111 ms     |
| 1/16     | 0.25      | 125 ms     | 187 ms    | 83 ms      |

All values fall within the existing delay line capacity (max ~5s with multiplier).

### Clamping Strategy for Tempo Sync

```cpp
// In processBlock, after computing synced base delay:
// The base delay parameter range is 10-150ms, but synced values can exceed this.
// At 40 BPM with 1/2 note: 60000/40 * 2.0 = 3000ms
// With multiplier 33x: 3000 * 33 = 99000ms -- way beyond buffer.
// Clamp base delay before multiplier to prevent overflow.
effectiveBaseDelay = std::min(effectiveBaseDelay, 150.0f);
// The multiplier then scales this just like free-running mode.
// Total delay = effectiveBaseDelay * multiplier * tapPosition
// DelayLine max = 150 * 33 * sr / 1000 samples (already allocated)
```

### State Persistence Pattern (Version Bump)

```cpp
void ZeitraumProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr) { jassertfalse; return; }
    xml->setAttribute("pluginVersion", 3);  // bumped from 2
    copyXmlToBinary(*xml, destData);
}
```

No migration code in `setStateInformation` -- APVTS handles missing TEMPO_SYNC and NOTE_DIV by using their default values (false and 0 respectively).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `getCurrentPosition()` returning bool | `getPosition()` returning `Optional<PositionInfo>` | JUCE 7+ | Must use Optional checks; old API deprecated |
| Flat parameter lists | AudioProcessorParameterGroup | JUCE 5.4+ | VST3/AUv3 show organized parameter trees |
| `AudioParameterFloat(id, name, range, default, label)` | `AudioParameterFloat(id, name, range, default, Attributes{})` | JUCE 7+ | Both constructors available; simple label still works |

## Open Questions

1. **Clamping vs. extending delay range in sync mode**
   - What we know: The current base delay range is 10-150ms. With tempo sync, a 1/2 note at 60 BPM = 2000ms exceeds this.
   - What's unclear: Should the effective base delay be clamped to 150ms (safe, consistent with existing architecture), or should we allow the synced value to pass through unclamped since the delay line buffer can handle up to 150ms * 33x = ~5s total?
   - Recommendation: Allow unclamped synced base delay values since the delay line buffer is already sized for 150ms * 33x. The multiplier effectively gives headroom. A 1/2 note at 60 BPM (2000ms) with multiplier 1x = 2000ms total delay, well within the ~5s buffer. Only clamp to prevent exceeding the delay line maximum (150 * 33 = 4950ms total delay time per tap).

2. **Fallback BPM behavior**
   - What we know: Host may not provide BPM (standalone mode, plugin scanning).
   - Recommendation: Fall back to 120 BPM (industry standard default). No need to disable sync -- just use the fallback. This matches the deprecated `CurrentPositionInfo` default of 120.0 BPM.

## Sources

### Primary (HIGH confidence)
- JUCE 8.0.12 source code (lib/JUCE) - AudioProcessorParameterGroup.h, AudioPlayHead.h, AudioProcessorValueTreeState.h
- Existing project codebase - PluginProcessor.cpp, DelayEngine.h (current parameter layout, state management, smoothing architecture)

### Secondary (MEDIUM confidence)
- JUCE AudioPlayHead::PositionInfo documentation pattern - Optional-based API is the modern standard in JUCE 8

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All components are existing JUCE APIs already used in the project
- Architecture: HIGH - Parameter grouping is well-documented JUCE pattern; tempo sync is simple math conversion feeding existing DSP path
- Pitfalls: HIGH - Based on direct code inspection of existing architecture and JUCE API constraints

**Research date:** 2026-03-07
**Valid until:** 2026-04-07 (stable JUCE APIs, project-specific patterns well established)
