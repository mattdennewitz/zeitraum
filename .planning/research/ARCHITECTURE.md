# Architecture Research

**Domain:** Preset randomizer integration into JUCE 8 APVTS plugin (Zeitraum v1.2)
**Researched:** 2026-03-10
**Confidence:** HIGH — based on direct inspection of existing source code and JUCE's documented parameter model

---

## Standard Architecture

### System Overview (Existing + New)

```
┌─────────────────────────────────────────────────────────────────┐
│                         Message Thread                           │
│                                                                  │
│  ┌──────────────┐  ┌──────────────────────────────────────────┐  │
│  │  ZeitraumEditor                                            │  │
│  │  ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌─────────────┐ │  │
│  │  │ TopBar  │  │TapColumn│  │Feedback  │  │[NEW]        │ │  │
│  │  │(sliders │  │[x8]     │  │MatrixEd. │  │RandomizeBtn │ │  │
│  │  │ combos) │  │         │  │          │  │             │ │  │
│  │  └────┬────┘  └────┬────┘  └────┬─────┘  └──────┬──────┘ │  │
│  │       │            │            │                │         │  │
│  └───────┼────────────┼────────────┼────────────────┼─────────┘  │
│          │ read/write │ via        │ ParameterAttach│ment         │
│          ▼            ▼            ▼                ▼             │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │              APVTS (38 parameters + [NEW] RANDOMIZE)       │   │
│  │         juce::AudioProcessorValueTreeState                  │   │
│  └───────────────────────────┬───────────────────────────────┘   │
│                              │ atomic<float>* pointers            │
└──────────────────────────────┼─────────────────────────────────--┘
                               │ (lock-free reads)
┌──────────────────────────────▼──────────────────────────────────┐
│                          Audio Thread                             │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │ ZeitraumProcessor::processBlock()                         │    │
│  │                                                           │    │
│  │  [existing] read 38 param atomics → DelayEngine.process() │    │
│  │                                                           │    │
│  │  [NEW] detect RANDOMIZE trigger edge → apply random vals  │    │
│  │        via setValueNotifyingHost() on message thread      │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │  DelayEngine (dual-mono: L + R)                          │     │
│  │    DelayLine, TapReader[8], FeedbackMatrix,              │     │
│  │    FeedbackFilter, FeedbackSaturator, CharacterProcessor  │     │
│  └─────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Status |
|-----------|----------------|--------|
| **ZeitraumProcessor** | JUCE lifecycle, parameter wiring, processBlock, randomizer dispatch | Existing — add randomizer trigger detection |
| **APVTS** | 38 thread-safe parameters, DAW automation, state XML | Existing — add 1 new RANDOMIZE trigger param |
| **ZeitraumEditor** | GUI composition, layout | Existing — add RandomizeButton |
| **TopBar** | Global parameter controls (delay, mix, etc.) | Existing — no change needed |
| **TapColumn / TapPositionBar / TapLevelFader** | Per-tap drag interaction, ParameterAttachment | Existing — no change needed |
| **FeedbackMatrixEditor / FeedbackGainCell** | Feedback routing controls, ParameterAttachment | Existing — no change needed |
| **[NEW] RandomizeButton** | Trigger randomization on click, calls processor method | New component |
| **[NEW] ParameterRandomizer** | Generates random values for all 38 params, calls setValueNotifyingHost | New class in PluginProcessor.h |

---

## Recommended Project Structure

```
src/
├── PluginProcessor.h/cpp    # Add: RANDOMIZE param, randomizeAllParameters(),
│                            #      trigger detection in processBlock
├── PluginEditor.h/cpp       # Add: RandomizeButton wiring
├── dsp/                     # No changes needed
│   ├── DelayEngine.h
│   ├── TapReader.h
│   ├── FeedbackMatrix.h
│   ├── FeedbackFilter.h
│   ├── FeedbackSaturator.h
│   ├── CharacterProcessor.h
│   └── OnePoleSmooth.h
└── ui/
    ├── ZeitraumLookAndFeel.h
    ├── TopBar.h             # No changes needed
    ├── TapColumn.h          # No changes needed
    ├── TapPositionBar.h     # No changes needed
    ├── TapLevelFader.h      # No changes needed
    ├── FeedbackMatrixEditor.h  # No changes needed
    └── FeedbackGainCell.h   # No changes needed
```

### Structure Rationale

- **No new DSP files needed:** The randomizer is a parameter-writing operation, not a DSP operation. All sound generation already works correctly from parameters. Randomization just drives new values into the existing parameter tree.
- **ParameterRandomizer as a method, not a class:** Given its simplicity (generate 38 random floats, call setValueNotifyingHost 38 times), a standalone `randomizeAllParameters()` method in ZeitraumProcessor is sufficient. A separate class would be overengineering for this scope.
- **RandomizeButton in PluginEditor, not TopBar:** The button is a one-off action trigger, not a persistent control. Adding it directly to ZeitraumEditor avoids contaminating TopBar with non-parameter UI concerns.

---

## Architectural Patterns

### Pattern 1: Apply-and-Reset Trigger Parameter

**What:** A parameter whose value is meaningless at rest (0 = idle), and whose transition from 0 to non-zero is the event of interest. The processor detects the rising edge, performs the action, then resets the parameter to 0 via `setValueNotifyingHost()`.

**When to use:** When an action needs to be automatable from a DAW but doesn't have a persistent state. The OUTPUT_MIX preset selector in Zeitraum already uses this exact pattern.

**Confidence:** HIGH — this is the existing pattern at line 234-258 of PluginProcessor.cpp.

**Existing precedent (verbatim from processBlock):**
```cpp
int outputMix = static_cast<int>(outputMixParam->load());
if (outputMix > 0)
{
    // ... apply preset ...

    // Reset selector back to Manual
    outputMixParamObj->setValueNotifyingHost(0.0f);
}
```

**Apply to RANDOMIZE:**
```cpp
// In createParameterLayout():
globalGroup->addChild(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"RANDOMIZE", 1}, "Randomize", false));

// In processBlock():
if (randomizeParam->load() > 0.5f)
{
    // Must dispatch to message thread — setValueNotifyingHost is not audio-thread-safe
    juce::MessageManager::callAsync([this]() {
        randomizeAllParameters();
    });
    randomizeParamObj->setValueNotifyingHost(0.0f);
}
```

**Trade-offs:**
- Pro: Automatable, state-saves cleanly (param resets to 0 immediately), follows existing project pattern.
- Pro: Zero new infrastructure — same mechanism already proven.
- Con: One-sample latency before randomize fires (detectected in processBlock, dispatched async). Imperceptible in practice.
- Con: `callAsync` from processBlock is technically calling a GUI thread function from the audio thread, but JUCE's `callAsync` is explicitly documented as safe to call from any thread. This is the correct approach.

### Pattern 2: Batch Parameter Update via setValueNotifyingHost

**What:** Writing multiple parameter values in a loop from the message thread, using `setValueNotifyingHost()` on each `RangedAudioParameter*` obtained via `apvts.getParameter()`.

**When to use:** Whenever a "preset load" operation (of any kind) needs to atomically update multiple parameters while keeping the DAW informed (automation lanes update, undo history records the change).

**Confidence:** HIGH — `recallTapPreset()` in PluginProcessor.cpp already does this for tap positions (lines 362-386). The randomizer extends it to all 38 parameters.

**Existing precedent (from recallTapPreset):**
```cpp
if (auto* param = apvts.getParameter(paramId))
    param->setValueNotifyingHost(param->convertTo0to1(value));
```

**Full randomizer implementation pattern:**
```cpp
void ZeitraumProcessor::randomizeAllParameters()
{
    // Must be called on the message thread
    jassert(juce::MessageManager::existsAndIsCurrentThread());

    juce::Random rng;
    rng.setSeedRandomly();

    auto randomizeFloat = [&](const juce::String& id) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(rng.nextFloat()); // 0..1 normalized
    };

    auto randomizeBool = [&](const juce::String& id) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(rng.nextBool() ? 1.0f : 0.0f);
    };

    // Global
    randomizeFloat("BASE_DELAY");
    randomizeFloat("MULTIPLIER");
    randomizeFloat("MIX");
    randomizeFloat("CHARACTER");

    // Taps (sorted positions prevent meaningless crossed-tap ordering)
    float positions[8];
    for (int i = 0; i < 8; ++i) positions[i] = rng.nextFloat();
    std::sort(positions, positions + 8); // ascending order preserves musical sense
    for (int i = 0; i < 8; ++i) {
        if (auto* p = apvts.getParameter("TAP" + juce::String(i + 1) + "_POS"))
            p->setValueNotifyingHost(positions[i]);
        randomizeFloat("TAP" + juce::String(i + 1) + "_LEVEL");
    }

    // Feedback (sparse: most gains should stay 0 for musical results)
    for (int i = 1; i <= 8; ++i)
        randomizeFloat("FB_TAP" + juce::String(i));
    randomizeFloat("FB_ODD");
    randomizeFloat("FB_EVEN");
    randomizeFloat("FB_RISING");
    randomizeFloat("FB_FALLING");

    // Feedback filters
    randomizeFloat("FB_HP_FREQ");
    randomizeFloat("FB_LP_FREQ");
    randomizeBool("FB_HP_ON");
    randomizeBool("FB_LP_ON");

    // NOTE: Skip RANDOMIZE itself, QUANTIZE, TEMPO_SYNC, NOTE_DIV, OUTPUT_MIX
    // These are mode/trigger parameters, not sound-shaping parameters.
}
```

**Trade-offs:**
- Pro: `setValueNotifyingHost()` updates the DAW automation lane, records an undo-able event in the DAW, and propagates through APVTS to the UI. All 38+ `ParameterAttachment` instances in the UI will repaint automatically.
- Pro: No manual UI refresh code needed anywhere — ParameterAttachment listeners fire automatically.
- Pro: State saves correctly because APVTS holds the current (randomized) values.
- Con: 38 individual calls vs. a single atomic state replacement. For a one-time action this is fine; for per-sample parameter modulation it would be far too expensive.

### Pattern 3: Sorted Tap Position Randomization

**What:** When randomizing tap positions, sort them in ascending order before writing so that Tap 1 is always the earliest tap and Tap 8 the latest.

**When to use:** Tap positions have an implicit semantic ordering (they are positions along a shared delay line from earliest to latest). Crossing them (Tap 3 before Tap 1 in time) is meaningless in this architecture and would confuse the user's mental model.

**Confidence:** HIGH — this is a design constraint from the existing architecture, confirmed by how tap positions are read in processBlock (they are indexed 0..7 by index, not sorted dynamically).

**Implementation:** See the `std::sort` call in Pattern 2. Five lines.

**Trade-offs:**
- Pro: Musically sensible. The display (TapPositionBar heights) will always read low-to-high visually.
- Con: Slightly reduces the randomization space (only ordered permutations of 8 positions). This is the correct trade-off.

---

## Data Flow

### Randomize Request Flow (Button Click)

```
User clicks RandomizeButton (message thread)
    ↓
ZeitraumProcessor::randomizeAllParameters() called directly
    ↓
Loop: apvts.getParameter(id)->setValueNotifyingHost(normalizedValue)
    ↓ (for each parameter)
APVTS notifies all listeners
    ↓
ParameterAttachment callbacks fire → UI components repaint
    ↓
APVTS atomic<float>* values updated
    ↓ (next processBlock call)
Audio thread reads new values via .load()
```

### Randomize Request Flow (DAW Automation)

```
DAW writes RANDOMIZE parameter to 1.0 (message thread, via automation lane)
    ↓
APVTS updates atomic<float>* for RANDOMIZE
    ↓ (next processBlock)
Audio thread: randomizeParam->load() > 0.5f detected
    ↓
juce::MessageManager::callAsync([this]{ randomizeAllParameters(); })
    ↓
randomizeParamObj->setValueNotifyingHost(0.0f)  // reset trigger
    ↓
Message thread: randomizeAllParameters() runs (same as button click path)
```

### State Persistence Flow (No Changes)

```
DAW saves session:
    getStateInformation() → apvts.copyState() → XML → MemoryBlock
    (RANDOMIZE param is 0 at save time — it always resets immediately)

DAW loads session:
    setStateInformation() → XML → apvts.replaceState()
    (All randomized values restore correctly — they are regular param values)
```

### UI Update Flow (Automatic, No New Code)

```
setValueNotifyingHost() called for TAP3_POS
    ↓
APVTS fires ValueTree::Listener callbacks
    ↓
ParameterAttachment (in TapPositionBar for Tap 3) fires setValue callback
    ↓
TapPositionBar::setValue() updates currentValue, calls repaint()
    ↓
Component redraws at next paint cycle
```

This is the existing JUCE ParameterAttachment contract. No new listener code needed anywhere in the UI layer.

---

## Integration Points

### New vs Modified Components

| Component | Change Type | What Changes |
|-----------|-------------|--------------|
| **PluginProcessor** | Modified | Add RANDOMIZE param to layout, add `randomizeAllParameters()` method, add trigger detection in processBlock, add 2 new cached pointers (`randomizeParam`, `randomizeParamObj`) |
| **ZeitraumEditor** | Modified | Add RandomizeButton member, wire onClick to `processorRef.randomizeAllParameters()` |
| **createParameterLayout()** | Modified | Add AudioParameterBool `{"RANDOMIZE", 1}` to global group |
| **PluginProcessor.h** | Modified | Declare `randomizeAllParameters()`, add `randomizeParam` and `randomizeParamObj` pointers |
| **RandomizeButton** | New (optional) | Could be a plain `juce::TextButton` inline in ZeitraumEditor — no separate class needed unless LookAndFeel customization is required |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| ZeitraumEditor → ZeitraumProcessor | Direct method call `processorRef.randomizeAllParameters()` from message thread | Same pattern as `processorRef.saveTapPreset()` / `recallTapPreset()` already used |
| ZeitraumProcessor → APVTS | `apvts.getParameter(id)->setValueNotifyingHost(v)` | Must be message thread. `randomizeAllParameters()` is always called on message thread |
| APVTS → UI components | ParameterAttachment callbacks (automatic, no new code) | All 38 UI bindings update automatically when their parameter values change |
| Audio thread → Message thread (automation path) | `juce::MessageManager::callAsync()` | Correct JUCE pattern for audio→message thread dispatch; safe to call from processBlock |

---

## Anti-Patterns

### Anti-Pattern 1: Calling setValueNotifyingHost from the Audio Thread

**What people do:** Detect the RANDOMIZE trigger in processBlock and immediately call `setValueNotifyingHost()` on 38 parameters there.

**Why it's wrong:** `setValueNotifyingHost()` fires ValueTree listeners, which can call arbitrary code including GUI repaints. This is not realtime-safe and will cause priority inversion or deadlocks when called from the audio thread. JUCE documents that `setValueNotifyingHost()` must be called on the message thread.

**Do this instead:** From processBlock, dispatch with `juce::MessageManager::callAsync()`, then call `randomizeAllParameters()` from the lambda. This is safe because `callAsync` is documented as callable from any thread and the lambda runs on the message thread.

### Anti-Pattern 2: Using apvts.replaceState() for Randomization

**What people do:** Construct a new ValueTree with random parameter values and call `apvts.replaceState(newTree)` to atomically swap all parameters.

**Why it's wrong:** `replaceState()` does not fire individual parameter change notifications the same way `setValueNotifyingHost()` does. The DAW does not record individual parameter automation events. Additionally, `replaceState()` is designed for session restore, not for user-triggered parameter changes — the undo granularity is wrong.

**Do this instead:** Call `setValueNotifyingHost()` individually for each parameter. This fires the correct JUCE notifications, records proper undo history in the DAW, and updates automation lanes.

### Anti-Pattern 3: Bypassing ParameterAttachment with Manual UI Refresh

**What people do:** After randomizing parameters, manually iterate through all UI components and push new values to them (e.g., calling `slider.setValue()`).

**Why it's wrong:** This duplicates the job of `ParameterAttachment`. It will fight with ParameterAttachment (which also listens and updates). It requires knowing about all UI components from the processor — a layering violation. It's also fragile when UI components are added or rearranged.

**Do this instead:** Trust the ParameterAttachment contract. `setValueNotifyingHost()` fires the APVTS listener, which fires the ParameterAttachment callback, which calls the component's `setValue()` lambda, which calls `repaint()`. The entire chain is already wired. No manual UI refresh code is needed.

### Anti-Pattern 4: Randomizing Mode/Trigger Parameters

**What people do:** Include RANDOMIZE, OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, and NOTE_DIV in the randomization sweep.

**Why it's wrong:** RANDOMIZE is a trigger that resets to 0 immediately — randomizing it would cause infinite re-triggering or silent failure. OUTPUT_MIX is also a trigger. TEMPO_SYNC and QUANTIZE are modes that interact with base delay calculation in non-obvious ways. Randomizing them would cause confusing behavior (e.g., tempo sync enabled with random note division overrides the base delay the user sees).

**Do this instead:** Maintain an explicit allowlist of parameters to randomize. All parameters in groups `global` (BASE_DELAY, MULTIPLIER, MIX, CHARACTER), `tap1`..`tap8` (POS, LEVEL), and `feedback` (FB_TAP*, FB_ODD, FB_EVEN, FB_RISING, FB_FALLING, FB_HP_FREQ, FB_LP_FREQ, FB_HP_ON, FB_LP_ON) are sound-shaping and should be randomized. QUANTIZE, TEMPO_SYNC, NOTE_DIV, OUTPUT_MIX, RANDOMIZE stay at their current values.

---

## Build Order

Dependencies are minimal — this is a small feature addition to an existing, working plugin. Order matters only for testability.

### Step 1: Parameter Addition (no UI, no behavior)

Add `RANDOMIZE` bool parameter to `createParameterLayout()` in the global group. Cache `randomizeParam` and `randomizeParamObj` pointers in the constructor. Add `randomizeAllParameters()` stub that does nothing.

**Verify:** Plugin still builds and runs. Existing behavior unchanged. RANDOMIZE parameter visible in DAW automation lane.

### Step 2: Randomizer Logic

Implement `randomizeAllParameters()` with the sorted-positions approach. Keep the explicit allowlist of parameters to randomize.

**Verify:** Call from a debug menu or temporary button in ZeitraumEditor. All 38 parameters update. UI repaints correctly without manual refresh. State saves and restores correctly (DAW save/load round-trip). No audio glitches during randomize.

### Step 3: GUI Button

Add `RandomizeButton` (plain `juce::TextButton`, styled via `ZeitraumLookAndFeel`) to ZeitraumEditor. Wire `onClick` to `processorRef.randomizeAllParameters()`.

**Verify:** Button click triggers randomize. Button placement fits existing layout without crowding controls.

### Step 4: Automation Trigger Path (optional — only if DAW automation is required for v1.2)

Add processBlock trigger detection: `if (randomizeParam->load() > 0.5f) { callAsync(...); reset; }`. This enables DAW automation of RANDOMIZE.

**Verify:** Drawing an automation point on the RANDOMIZE lane triggers randomize. Parameter resets to 0 after firing. No double-triggering.

**Rationale for ordering:** Steps 1-3 deliver the user-facing feature (GUI button). Step 4 adds DAW automation support. This ordering lets testing of the core behavior happen without the complexity of the automation path. The automation path carries the most subtle correctness risks (audio-thread to message-thread dispatch), so it should be added and tested separately.

---

## Scaling Considerations

This feature is a one-shot action — scaling is not a concern. The only performance-relevant note is that `randomizeAllParameters()` makes 38 calls to `setValueNotifyingHost()`, which fires 38 ValueTree listener callbacks, which trigger 38 component repaints. At a human-triggered frame rate this is invisible. The total execution time is in the microseconds range.

---

## Sources

- `/Users/matt/src/multi-tap-delay/src/PluginProcessor.cpp` — Direct inspection: OUTPUT_MIX apply-and-reset pattern (lines 234-258), recallTapPreset batch setValueNotifyingHost (lines 362-386). HIGH confidence.
- `/Users/matt/src/multi-tap-delay/src/ui/TapPositionBar.h` — Direct inspection: ParameterAttachment contract, setValue callback, repaint chain. HIGH confidence.
- `/Users/matt/src/multi-tap-delay/src/ui/FeedbackGainCell.h` — Direct inspection: ParameterAttachment pattern with ignoreCallbacks guard. HIGH confidence.
- JUCE `juce::ParameterAttachment` documentation: `setValueNotifyingHost()` must be message thread; `MessageManager::callAsync()` is safe from any thread. HIGH confidence (consistent with observed JUCE behavior in existing codebase).
- JUCE `juce::Random` class: `nextFloat()` returns 0..1, `setSeedRandomly()` for non-deterministic seeds. HIGH confidence.

---
*Architecture research for: Preset randomizer integration into Zeitraum JUCE plugin*
*Researched: 2026-03-10*
