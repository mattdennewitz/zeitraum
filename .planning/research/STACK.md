# Stack Research

**Domain:** JUCE audio plugin — preset randomizer feature
**Researched:** 2026-03-10
**Confidence:** HIGH (all findings verified against JUCE 8.0.12 source in `lib/JUCE`)

---

## Scope

This is a **subsequent milestone** stack document. The existing stack (JUCE 8.0.12, C++17,
CMake+Ninja, Catch2 v3.7.1, APVTS) is validated and unchanged. This document covers only the
**new capabilities required for the preset randomizer feature**:

- Random number generation
- Automatable trigger parameter
- Batch parameter update across all 38 parameters
- GUI randomize button

No new dependencies are required. Everything needed is already in JUCE.

---

## New Capabilities Required

### Random Number Generation

| Technology | Source | Purpose | Why Recommended |
|------------|--------|---------|-----------------|
| `juce::Random` | `juce_core` (already linked) | Generate random float values for each parameter | Verified in `lib/JUCE/modules/juce_core/maths/juce_Random.h`. Thread-safe global accessor available. No new dependency. |

**API surface needed:**

```cpp
// Default constructor calls setSeedRandomly() internally — use this
juce::Random rng;

// Returns float in [0.0, 1.0)
float rng.nextFloat();

// Returns bool
bool rng.nextBool();

// Returns int in [0, maxValue)
int rng.nextInt(int maxValue);

// Per-call reseeding if reproducibility matters later
rng.setSeed(juce::Time::currentTimeMillis());
```

`juce::Random::getSystemRandom()` returns a per-thread singleton — acceptable for a GUI
callback but not useful here since the randomizer runs on the message thread from a button click.
Instantiate one `juce::Random` as a member of `ZeitraumProcessor` (or the editor) for clarity.

**Confidence: HIGH** — Read directly from `juce_Random.h` in the project's own JUCE submodule.

---

### Automatable Trigger Parameter

The randomizer needs a DAW-automatable parameter so users can automate when randomization
fires (e.g., trigger it every 4 bars). The `OUTPUT_MIX` apply-and-reset pattern already in
`PluginProcessor.cpp` is the exact model to follow.

| Mechanism | Where | Purpose |
|-----------|-------|---------|
| `AudioParameterBool` (momentary) | APVTS layout | Automatable trigger that resets to false after firing |
| `RangedAudioParameter::setValueNotifyingHost` | processBlock | Apply random values to all params, then reset trigger |
| `beginChangeGesture` / `endChangeGesture` | GUI button handler | Wrap the button's programmatic change for proper automation recording |

**Trigger parameter declaration** (add to `createParameterLayout()` in the global group):

```cpp
globalGroup->addChild(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"RANDOMIZE", 2},  // version 2 — new in v1.2
    "Randomize",
    false));
```

Use `ParameterID{"RANDOMIZE", 2}` (version hint 2). The existing 38 parameters all use version
hint 1. Adding a new parameter requires version hint 2 so AU hosts (Logic, GarageBand) maintain
correct parameter ordering — this is explicitly documented in `juce_AudioProcessorParameter.h`.

**processBlock detection** (mirrors OUTPUT_MIX pattern already in the codebase):

```cpp
// In processBlock, before DSP:
if (randomizeParam->load() > 0.5f)
{
    applyRandomization();
    randomizeParamObj->setValueNotifyingHost(0.0f);  // reset to false
}
```

**Confidence: HIGH** — Pattern is verified directly in existing `PluginProcessor.cpp` lines
234-258. The `beginChangeGesture`/`endChangeGesture` requirement is documented in
`juce_AudioProcessorParameter.h` lines 131-156.

---

### Batch Parameter Update

The randomizer must update all ~38 parameters atomically from the message thread (GUI button)
or audio thread (processBlock automation trigger). These are different contexts with different rules.

#### Context A: GUI Button (message thread)

When the user clicks "Randomize", the button callback runs on the message thread. Use the full
gesture protocol so DAW automation records the change correctly:

```cpp
void randomizeFromGUI()
{
    juce::Random rng;

    // For each parameter being randomized:
    auto* param = processorRef.apvts.getParameter("TAP1_POS");
    param->beginChangeGesture();
    param->setValueNotifyingHost(rng.nextFloat());  // already normalized [0,1)
    param->endChangeGesture();
    // ... repeat for all parameters
}
```

`beginChangeGesture` / `endChangeGesture` bracket tells the host automation is starting/ending.
Omitting them means the DAW may not record the change into an automation lane. This is documented
in `juce_AudioProcessorParameter.h` lines 143-156 and the `setValueNotifyingHost` docstring
(line 131-140).

#### Context B: processBlock (audio thread trigger via automation)

When the RANDOMIZE parameter fires from DAW automation, it is detected in processBlock. In this
context, `setValueNotifyingHost` is legal (it is called from the audio thread when the host
changes parameters) but `beginChangeGesture`/`endChangeGesture` should not be called from the
audio thread.

The simplest approach: detect the trigger in processBlock, set an atomic flag, and apply the
randomization on the next message thread cycle via `juce::MessageManager::callAsync` or
`juce::AsyncUpdater`. This keeps randomization logic off the audio thread entirely.

```cpp
// In processBlock:
if (randomizeParam->load() > 0.5f)
{
    randomizeParamObj->setValueNotifyingHost(0.0f);  // reset trigger
    randomizePending.store(true);  // signal message thread
}

// In processor or editor (message thread, via AsyncUpdater):
void handleAsyncUpdate() override
{
    if (randomizePending.exchange(false))
        applyFullRandomization();
}
```

This pattern avoids gesture calls on the audio thread and keeps randomization deterministic
relative to the message thread flush cycle.

#### NormalisableRange and convertTo0to1

Parameters use `NormalisableRange` with skew factors (e.g., BASE_DELAY uses skew 0.5,
FB_HP_FREQ uses skew 0.3). `setValueNotifyingHost` expects a **normalized [0,1] value**, not
the physical value.

Two options for generating random values:

**Option 1 (preferred): Generate directly in normalized space.**
`nextFloat()` returns [0,1) — pass directly to `setValueNotifyingHost`. The NormalisableRange
will map it to the physical range when the audio thread reads `getRawParameterValue`. This
produces uniform distribution in normalized space, which means more samples at low values for
parameters with skew < 1 (delay time, filter frequency). This may be musically preferable —
more short delays than long, more low frequencies than high.

**Option 2: Generate in physical space, then normalize.**
Use `param->convertTo0to1(physicalValue)` (verified in `juce_RangedAudioParameter.h` line 123)
to convert a physical value to normalized before passing to `setValueNotifyingHost`. Use this
if uniform distribution in physical space is desired (e.g., random delay times equally likely
to be anywhere in 10-150ms).

For AudioParameterBool, `nextBool()` maps to 0.0f (false) or 1.0f (true) directly.
For AudioParameterChoice (NOTE_DIV), use `rng.nextInt(numChoices)` and normalize to [0,1].

**Confidence: HIGH** — `convertTo0to1` confirmed in `juce_RangedAudioParameter.h`. The
normalized-space behavior of `setValueNotifyingHost` is standard JUCE documentation.

---

### GUI Randomize Button

No new component needed. Use the existing `juce::TextButton` (already used for `savePresetButton`
in `ZeitraumEditor`). Wire it with a `juce::TextButton::onClick` lambda.

```cpp
// In ZeitraumEditor:
juce::TextButton randomizeButton { "Randomize" };

// In constructor or resized:
randomizeButton.onClick = [this] { applyRandomization(); };
addAndMakeVisible(randomizeButton);
```

No `ButtonAttachment` needed — the randomizer is a momentary action, not a persistent parameter
state to reflect in the GUI.

**Confidence: HIGH** — Matches existing `savePresetButton` pattern in `ZeitraumEditor`.

---

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `std::mt19937` or `std::random_device` | Works fine, but adds no benefit over `juce::Random` which is already available and well-tested in JUCE context | `juce::Random` |
| `juce::AsyncUpdater` as a standalone class | Unnecessary if processor or editor already inherits it — check first | Inherit in `ZeitraumProcessor` or use `juce::MessageManager::callAsync` |
| `ParameterAttachment` for the randomize button | Overkill for a momentary trigger action; adds listener lifecycle complexity | Direct `onClick` lambda calling processor method |
| AudioUnit meta-parameter flag | `isMetaParameter()` is a hint that a param controls other params. Theoretically correct for the trigger, but most hosts ignore it and it complicates AU validation | Leave as default (false) |
| New JUCE modules | `juce::Random` is in `juce_core`, already linked. No additional CMake changes required | Existing linked modules |

---

## Integration Points in Existing Code

| Existing Component | How Randomizer Integrates |
|--------------------|--------------------------|
| `PluginProcessor.h` | Add `std::atomic<float>* randomizeParam = nullptr` and `juce::RangedAudioParameter* randomizeParamObj = nullptr`. Add `std::atomic<bool> randomizePending{false}`. Add `void applyFullRandomization()` method. |
| `PluginProcessor.cpp` — `createParameterLayout()` | Add `AudioParameterBool{"RANDOMIZE", 2}` to global group. |
| `PluginProcessor.cpp` — constructor | Cache `randomizeParam` and `randomizeParamObj` via `getRawParameterValue` / `getParameter`. |
| `PluginProcessor.cpp` — `processBlock` | Detect trigger, reset it, set `randomizePending` flag. |
| `PluginProcessor.cpp` — `applyFullRandomization()` | New method. Generate random values and call `setValueNotifyingHost` for each of the ~38 parameters. Wrap in `beginChangeGesture`/`endChangeGesture` if called from message thread. |
| `PluginEditor.h/cpp` | Add `juce::TextButton randomizeButton`. Wire `onClick` to call processor method or fire directly through APVTS. |
| `getStateInformation` | No change needed — RANDOMIZE param saves/restores as false, which is correct. |
| XML `pluginVersion` attribute | No change — state version 3 is unchanged; no migration needed since RANDOMIZE defaults to false on restore. |

---

## Parameter Version Hint Requirement

**Critical for Logic Pro / GarageBand AU compatibility.**

The existing 38 parameters all use version hint `1`. The new RANDOMIZE parameter must use
version hint `2`. This is mandatory per the documentation in `juce_AudioProcessorParameter.h`
(lines 46-98): AU hosts sort parameters by version hint then by string ID hash. Adding a
parameter with hint `1` would interleave it with existing parameters and break saved automation.

```cpp
// Correct:
juce::ParameterID{"RANDOMIZE", 2}

// Wrong — breaks AU automation ordering:
juce::ParameterID{"RANDOMIZE", 1}
```

**Confidence: HIGH** — Directly documented in `juce_AudioProcessorParameter.h` in this project's
JUCE submodule.

---

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| Apply-and-reset `AudioParameterBool` for trigger | Keep it permanently "on" when engaged | Apply-and-reset is the established pattern in this codebase (OUTPUT_MIX). It avoids state persistence issues and automation ambiguity. |
| `juce::Random` member instance | `juce::Random::getSystemRandom()` | System random is a per-thread singleton. Fine for the message thread but semantically cleaner to own the RNG in the class that uses it. |
| Normalized [0,1] random generation | Physical-space random + `convertTo0to1` | Normalized space is simpler and produces musically useful results (skew causes perceptually appropriate distribution). Use physical-space only if explicit range constraints needed. |
| `AsyncUpdater` for cross-thread trigger | `MessageManager::callAsync` | Either works. `AsyncUpdater` is more conventional for processor classes; `callAsync` is simpler for one-off triggers. Choose based on whether the processor will accumulate multiple pending randomizations. |

---

## Version Compatibility

| Parameter | Version Hint | Notes |
|-----------|-------------|-------|
| All existing 38 params | 1 | Unchanged |
| RANDOMIZE (new) | **2** | Must be 2 to preserve AU parameter ordering |

---

## Sources

- `lib/JUCE/modules/juce_core/maths/juce_Random.h` — `juce::Random` API verified (HIGH confidence)
- `lib/JUCE/modules/juce_audio_processors_headless/processors/juce_AudioProcessorParameter.h` — `setValueNotifyingHost`, `beginChangeGesture`, `endChangeGesture` verified (HIGH confidence)
- `lib/JUCE/modules/juce_audio_processors_headless/utilities/juce_RangedAudioParameter.h` — `convertTo0to1`, `convertFrom0to1` verified (HIGH confidence)
- `src/PluginProcessor.cpp` lines 234-258 — existing OUTPUT_MIX apply-and-reset pattern (HIGH confidence, in-codebase verification)
- `src/PluginProcessor.h` — existing parameter cache pattern (HIGH confidence)
- [JUCE Random class docs](https://docs.juce.com/master/classRandom.html) — confirms `nextFloat()` range [0, 1.0) (MEDIUM confidence, external)

---

*Stack research for: Preset randomizer feature, Zeitraum v1.2*
*Researched: 2026-03-10*
