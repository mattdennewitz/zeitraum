# Project Research Summary

**Project:** Zeitraum — Preset Randomizer (v1.2 milestone)
**Domain:** JUCE 8 audio plugin — parameter randomization feature
**Researched:** 2026-03-10
**Confidence:** HIGH

## Executive Summary

The preset randomizer for Zeitraum is a parameter-writing feature, not a DSP feature. All 38 parameters already exist in APVTS; randomization simply generates new values for them and calls `setValueNotifyingHost` on each. The existing codebase has two direct precedents to follow: the `OUTPUT_MIX` apply-and-reset trigger pattern and the `recallTapPreset` batch parameter update. No new dependencies, DSP classes, or modules are required — `juce::Random` is already in `juce_core`.

The recommended approach is a four-step build order: (1) add the RANDOMIZE bool parameter to the layout with version hint 2, (2) implement `randomizeAllParameters()` on `ZeitraumProcessor` with sorted tap positions and clamped ranges for feedback and wet/dry, (3) wire a `juce::TextButton` in `ZeitraumEditor`, and (4) add processBlock edge-detection to handle DAW automation. The automatable trigger is the primary differentiator — no competing delay plugin exposes it — but it carries the most implementation risk due to the audio-to-message-thread dispatch requirement.

The critical risks are well-understood and preventable. The two non-negotiable correctness requirements are: (a) `setValueNotifyingHost` must be called on the message thread, not the audio thread, and (b) trigger detection must use rising-edge logic (`prevTriggerState`) to prevent continuous re-randomization when a DAW automation lane holds the parameter high. A third risk — feedback instability from simultaneous high-gain randomization — is resolved by normalizing the sum of generated feedback gains to a safe ceiling before writing. All three are low-recovery-cost if addressed at the design stage.

## Key Findings

### Recommended Stack

No new dependencies are needed. The entire feature is implemented using existing JUCE infrastructure already linked in the project. `juce::Random` (in `juce_core`) provides the RNG. The APVTS parameter system handles state persistence, DAW automation, and UI sync automatically via `ParameterAttachment`. `juce::MessageManager::callAsync` handles the audio-to-message-thread dispatch required for the automation trigger path.

**Core technologies:**
- `juce::Random` — RNG for generating normalized [0,1] parameter values; already in `juce_core`, no CMake changes needed
- `juce::AudioParameterBool` with `ParameterID{"RANDOMIZE", 2}` — automatable momentary trigger; version hint 2 is mandatory to preserve AU parameter ordering in Logic/GarageBand
- `juce::MessageManager::callAsync` — safe cross-thread dispatch from audio thread to message thread; documented as callable from any thread
- `setValueNotifyingHost` on `RangedAudioParameter*` — correct call for batch parameter updates: notifies DAW automation, fires ParameterAttachment callbacks, updates UI automatically

### Expected Features

**Must have (table stakes):**
- GUI randomize button (dice icon / "RAND") — primary user-facing surface; every plugin with randomization has one
- Full parameter coverage: all 8 tap positions (sorted ascending), 8 tap levels, 12 feedback gains/sources, 2 filter cutoffs, 2 filter on/off, global multiplier and wet/dry
- Clamped ranges for dangerous parameters: feedback gains normalized so sum stays under ~80% of full scale; wet/dry clamped to [0.2, 0.9]; prevents runaway oscillation and "plugin disappeared" confusion
- Single undo transaction — must wrap all 38 changes; naive per-parameter undo floods the queue with 38 entries per press
- Automatable trigger parameter (RANDOMIZE) — the primary differentiator; enables DAW-driven evolving randomization

**Should have (competitive):**
- Per-group lock toggles (taps, levels, feedback, filters, globals) — most-requested refinement in synth randomizer communities; add after v1.2 based on user feedback
- Musical grid snap for tap positions — snap to subdivisions of the delay range; add if randomized results feel unmusical in practice

**Defer (v2+):**
- Randomize amount / deviation slider (±N% from current values) — adds state-tracking complexity; strong v2 candidate
- Seed-based deterministic randomization — elegant for automation, but requires seeded RNG and seed state persistence

### Architecture Approach

The randomizer integrates into two existing components only: `ZeitraumProcessor` (adds parameter, trigger detection, and `randomizeAllParameters()` method) and `ZeitraumEditor` (adds a `juce::TextButton` wired to that method). No DSP files change. No new UI classes are needed unless LookAndFeel customization is required. The ParameterAttachment contract means all 38 UI components (TapPositionBar, FeedbackGainCell, TapLevelFader, etc.) update automatically when `setValueNotifyingHost` is called — no manual UI refresh code needed.

**Major components:**
1. `ZeitraumProcessor::randomizeAllParameters()` — new method; generates random values with sorted tap positions and normalized feedback gains; calls `setValueNotifyingHost` for all 38 sound-shaping parameters from the message thread
2. `APVTS` (modified) — add one `AudioParameterBool{"RANDOMIZE", 2}` to the global group; handles state persistence, undo, DAW automation automatically
3. `ZeitraumEditor` (modified) — add `juce::TextButton randomizeButton`; wire `onClick` to `processorRef.randomizeAllParameters()`
4. `processBlock` (modified) — edge-detect RANDOMIZE trigger, dispatch `callAsync` to message thread, reset trigger to 0.0

### Critical Pitfalls

1. **Continuous re-randomization under DAW automation** — A naively written `if (param > 0.5f) randomize()` fires every processBlock while automation holds the value high. Prevent with `prevTriggerState` rising-edge detection. Initialize `prevTriggerState` from the saved parameter value in the constructor to also prevent session-load randomization.

2. **`setValueNotifyingHost` on the audio thread** — Calling 38 times from `processBlock` acquires mutexes, fires ValueTree listeners (including GUI repaints), and causes priority inversion or deadlocks. Use `juce::MessageManager::callAsync` dispatched from processBlock; the actual writes happen on the message thread. The existing `OUTPUT_MIX` pattern makes a single infrequent reset — extending it to 38 parameters at randomizer speed is not safe without the async dispatch.

3. **Feedback instability on randomize** — Drawing 12 feedback gains independently from U(0, max) frequently produces a sum exceeding 1.0, causing a transient spike before the FeedbackSaturator catches it. Prevent by normalizing: after generating raw gains, scale all by `safeMax / max(rawSum, safeMax)`. Five lines of code.

4. **Trigger parameter saves as 1.0 in session state** — If the trigger is 1.0 when a session is saved, and `prevTriggerState` initializes to `false`, the next session load fires randomization immediately. Prevent by initializing `prevTriggerState = (triggerParam->load() > 0.5f)` in the constructor after APVTS setup.

5. **Randomizing mode/trigger parameters** — Including RANDOMIZE, OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, or NOTE_DIV in the randomization sweep causes infinite retrigger or mode confusion. Maintain an explicit allowlist of sound-shaping parameters; skip all mode and trigger parameters.

## Implications for Roadmap

Based on research, the feature maps cleanly to a single phase with four internal steps ordered by testability and risk isolation.

### Phase 1: Parameter Foundation
**Rationale:** Adding the RANDOMIZE parameter and caching its pointers is zero-risk and validates the build before any behavior is added. The version hint 2 requirement for AU compatibility must be set here — it cannot be changed after the parameter exists in saved sessions.
**Delivers:** RANDOMIZE parameter visible in DAW automation lanes; plugin still builds and runs identically to v1.1
**Addresses:** AU compatibility (ParameterID version hint), state persistence (parameter saves/restores as 0)
**Avoids:** Pitfall 4 (trigger param in saved state) and AU parameter ordering regression

### Phase 2: Randomizer Logic
**Rationale:** Implement `randomizeAllParameters()` as a standalone method before wiring any UI or trigger. This isolates the algorithmic correctness (sorted tap positions, normalized feedback gains, clamped ranges) and allows testing via a debug call before the button exists.
**Delivers:** `randomizeAllParameters()` callable from a test or debug menu; all 38 parameters update correctly; UI repaints without manual refresh; state saves/restores correctly after randomize
**Addresses:** Full parameter coverage, clamped dangerous ranges, sorted tap positions
**Avoids:** Pitfall 3 (feedback instability), anti-pattern of using `apvts.replaceState()`, anti-pattern of manual UI refresh, anti-pattern of randomizing mode params

### Phase 3: GUI Button
**Rationale:** Wire the button only after the core logic is verified. Button is a `juce::TextButton` — no new class needed. This delivers the user-facing feature.
**Delivers:** Randomize button in ZeitraumEditor; click triggers randomize; button placement fits existing layout
**Implements:** TextButton pattern matching existing `savePresetButton`

### Phase 4: Automation Trigger Path
**Rationale:** Add processBlock trigger detection last — it carries the most correctness risk (audio-to-message-thread dispatch, edge detection, session load safety). Isolating it to a final step means the GUI button path is already verified before the more complex automation path is added.
**Delivers:** RANDOMIZE parameter in DAW automation lane triggers randomize; no double-triggering; trigger resets to 0 after firing; session load does not randomize
**Avoids:** Pitfall 1 (continuous re-randomize), Pitfall 2 (audio thread safety), Pitfall 4 (session load trigger)

### Phase Ordering Rationale

- Steps 1-3 deliver the user-visible feature (GUI button) before the automation path is added, enabling early integration testing
- The automation path (step 4) is sequenced last because it is the only path involving audio-thread-to-message-thread communication — the hardest correctness requirement
- Feedback gain normalization and clamped ranges belong in step 2 (logic phase) because they are algorithmic constraints, not UI concerns — fixing them late would require re-testing the entire parameter set
- All UI update behavior is automatic via ParameterAttachment — no dedicated "UI sync" step needed

### Research Flags

All phases have well-documented patterns verified against this project's own source code. No phases require additional research work.

- **Phase 1 (Parameter Foundation):** Standard APVTS parameter addition; version hint requirement documented in JUCE source in this repo
- **Phase 2 (Randomizer Logic):** Pattern directly present in `recallTapPreset` (lines 362-386 of PluginProcessor.cpp)
- **Phase 3 (GUI Button):** Matches existing `savePresetButton` pattern in ZeitraumEditor
- **Phase 4 (Automation Trigger):** Pattern directly present in `OUTPUT_MIX` (lines 234-258 of PluginProcessor.cpp); edge detection requirement is well-understood

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All APIs verified against `lib/JUCE` submodule in this repository; no new dependencies |
| Features | MEDIUM-HIGH | Table stakes and differentiators derived from community research (KVR, Vital, HISE forums) plus official competitor docs (Valhalla, Logic Pro) |
| Architecture | HIGH | Integration patterns verified directly against `PluginProcessor.cpp` lines 234-258 and 362-386; ParameterAttachment behavior confirmed from `FeedbackGainCell.h` and `TapPositionBar.h` |
| Pitfalls | HIGH | All 5 critical pitfalls derived from codebase inspection; each maps to a specific existing code pattern that would be extended incorrectly |

**Overall confidence:** HIGH

### Gaps to Address

- **UndoManager presence in APVTS:** FEATURES.md identifies single-undo-transaction as table stakes. The current APVTS constructor call was not verified to include a `juce::UndoManager*`. If absent, adding one is a constructor-signature change that must happen in Phase 1. Check `PluginProcessor.cpp` APVTS constructor call before starting Phase 2.
- **Quantize mode interaction:** PITFALLS.md flags that randomizing TAP_POS when QUANTIZE is enabled may need per-generation snapping to a 10ms grid, OR the DSP engine handles it silently on read. Verify `DelayEngine` behavior with quantized positions during Phase 2 testing.
- **Button placement in existing layout:** ARCHITECTURE.md recommends placing RandomizeButton directly in ZeitraumEditor rather than TopBar, but the exact layout coordinates depend on current editor bounds and existing control placement. Resolve during Phase 3.

## Sources

### Primary (HIGH confidence)
- `src/PluginProcessor.cpp` lines 234-258 — OUTPUT_MIX apply-and-reset trigger pattern
- `src/PluginProcessor.cpp` lines 362-386 — `recallTapPreset` batch `setValueNotifyingHost` pattern
- `src/ui/FeedbackGainCell.h`, `TapPositionBar.h` — ParameterAttachment + ignoreCallbacks contract
- `src/dsp/DelayEngine.h`, `FeedbackSaturator.h` — feedback gain read path; confirms no smoothing on feedback gains
- `lib/JUCE/modules/juce_core/maths/juce_Random.h` — `nextFloat()` range [0,1), `setSeedRandomly()`
- `lib/JUCE/modules/juce_audio_processors_headless/processors/juce_AudioProcessorParameter.h` — `setValueNotifyingHost`, `beginChangeGesture`, `endChangeGesture`, version hint ordering

### Secondary (MEDIUM confidence)
- [HISE forum: Undo/Redo with Randomization](https://forum.hise.audio/topic/9383/undo-redo-with-randomization/7) — undo queue flooding confirmed; transaction approach is the solution
- [Steinberg VST3 developer portal: Parameters and Automation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Parameters+Automation/Index.html) — "no automatable parameter shall influence another automatable parameter"
- [ValhallaUberMod TAPS Parameters](https://valhalladsp.com/2012/01/26/valhallaubermod-the-taps-parameters/) — TAPS Random behavior; precedent for tap-spacing randomization
- [KVR Audio: randomizer UX patterns](https://www.kvraudio.com/forum/viewtopic.php?t=565834) — per-group lock expectations, selective randomization

### Tertiary (LOW confidence)
- [Integra Audio: Top 12 Randomizer Plugins 2025](https://integraudio.com/12-best-randomizer-plugins/) — ecosystem survey; competitor feature set reference

---
*Research completed: 2026-03-10*
*Ready for roadmap: yes*
