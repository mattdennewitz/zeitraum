# Feature Research

**Domain:** Preset randomizer for multi-tap delay plugin (VST3/AU)
**Researched:** 2026-03-10
**Confidence:** MEDIUM-HIGH (web research verified against multiple sources; JUCE implementation details cross-checked with official forum posts)

---

> This file was updated for milestone v1.2. The sections below cover what users expect from a
> randomizer feature in an effects plugin, what separates good from bad implementations, and
> which sub-features to build vs. defer. Existing v1.0 features (8-tap engine, feedback matrix,
> GUI, automation, state persistence) are treated as dependencies, not scope.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume any randomizer will have. Missing these makes the feature feel broken or toy-like.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| GUI randomize button | Every plugin with randomization has an obvious trigger control; dice icon is the de facto standard | LOW | Single button; place near preset controls or in toolbar area |
| Randomizes all parameter groups | Users expect "randomize all" to mean all meaningful parameters — taps, levels, feedback, filters, wet/dry | LOW | 38 params across 5 groups; iterate all and set random values in range |
| Immediate audible result | Randomization applies instantly and is audible without extra steps | LOW | No preview mode needed; immediate commit is expected in effects plugins |
| Respects parameter ranges | Random values must stay within legal min/max bounds for every parameter | LOW | JUCE parameter range already defines this; sample uniformly from [min, max] |
| Smooth transitions to new values | Abrupt jumps create clicks/pops; parameters must transition, not teleport | MEDIUM | Tap positions use existing smoothing; feedback gains need same treatment. Smoothing is already in place for v1.0 parameters. |
| DAW undo support | Randomization should be undoable; users want to compare before/after | MEDIUM | Batch all changes as a single undo group using JUCE UndoManager; naive per-parameter changes flood the undo queue |
| Works at any playback state | Randomize button should work during playback and while stopped | LOW | State changes on message thread, audio thread reads smoothed values — no concern |

### Differentiators (Competitive Advantage)

Features that lift the randomizer from functional to delightful. Not required but add meaningful value given this plugin's architecture.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Automatable trigger parameter | DAW automation lane can trigger randomization — enables evolving textures over time without user interaction; no competing plugin does this well | MEDIUM | Expose a hidden "RANDOMIZE_TRIGGER" float param; fire on transition from 0 to non-zero; reset to 0 after processing. VST3 spec says no automatable param shall influence another automatable param — circumvent by having the trigger parameter write non-automatable shadow params, or document as an edge case. See Pitfalls. |
| Constrained tap-position randomization | Pure random tap positions produce musically unpleasant results (irregular clusters); constraining to rhythmically meaningful intervals produces usable sounds | MEDIUM | Options: (a) pure random within [0, max_delay], (b) random from a grid of musical subdivisions, (c) random with minimum spacing enforced between adjacent taps. Option (c) is lowest complexity with highest payoff. |
| Per-group lock/exclude | Users want to lock the current tap positions while randomizing only levels and feedback, or vice versa; this is the most-requested refinement in synth randomizer threads | MEDIUM | UI toggle per group (taps, levels, feedback routing, filters, globals). Adds ~5 toggle states to maintain. |
| Randomize amount / deviation control | Instead of full random, randomize within ±N% of current values; enables subtle variation vs. drastic reroll; used in NI Massive, Rob Papen Predator2 | HIGH | Requires storing "baseline" values and sampling from neighborhood. Not in scope for v1.2 but strong v2 candidate. |
| Seed-based deterministic randomization | Same seed always produces same result; users can save a seed to reproduce a happy accident; seed can itself be automated for reproducible evolving textures | HIGH | Requires seeded RNG, seed parameter exposed or stored in state. Architecturally elegant but adds state complexity. Defer to v2. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Full random tap positions without constraints | Simple to implement | Taps cluster randomly, produce metallic comb artifacts at irregular intervals, almost never sound musical. Users click once, get noise, never use the feature again. Valhalla UberMod explicitly keeps TAPS Random at low values to avoid this. | Enforce minimum inter-tap spacing OR snap to subdivisions of the delay range. Either produces far more usable results with minimal extra code. |
| Per-parameter undo entries | Naive approach — push one undo record per parameter | Floods undo queue with 38 entries per randomize press; Ctrl+Z becomes unusable. HISE forum confirmed this is a known failure mode. | Wrap all 38 changes in one UndoManager transaction (beginNewTransaction + all setValue calls). One undo restores all 38. |
| Randomize button as standard automatable parameter | Exposing a button as a normal automatable param | VST3 spec: no automatable parameter shall influence another automatable parameter. A randomize trigger that changes all other params violates this rule and causes undefined host behavior in some DAWs. | Use a non-automatable trigger param for the button; expose a separate automatable "RANDOMIZE_TRIGGER" float that is processed differently in processBlock (edge detection, fires randomization on message thread). |
| Randomizing wet/dry to extreme values | Maximally random | If wet/dry hits 0.0 or 1.0, the effect disappears or goes fully wet — users lose reference and think the plugin crashed. | Clamp wet/dry random range to [0.2, 0.9] or similar. Document this. |
| Randomizing feedback gain to max values | Maximally random | Feedback at or near 1.0 causes runaway oscillation; tanh saturation limits amplitude but the result is an unmusical sustained tone that is hard to escape. | Clamp feedback random range to [0.0, 0.7]. The existing saturation protects against physical damage but not against bad UX. |

## Feature Dependencies

```
Randomize Button (GUI)
    └──triggers──> Randomize Logic (message thread)
                       ├──writes──> Tap Position Parameters [existing smoothing]
                       ├──writes──> Tap Level Parameters [existing smoothing]
                       ├──writes──> Feedback Source + Gain Parameters [existing smoothing]
                       ├──writes──> Filter Parameters (HP/LP) [existing smoothing]
                       ├──writes──> Global Parameters (multiplier, wet/dry) [existing smoothing]
                       └──groups into──> UndoManager transaction [new dependency]

Automatable Trigger Parameter (new)
    └──detected in──> processBlock (edge: 0→nonzero)
                          └──posts to message thread──> Randomize Logic
                                                            └──resets trigger to 0 after firing

Per-Group Lock Toggles (optional differentiator)
    └──filters──> Randomize Logic (skips locked groups)

[All existing parameters] ──depend on──> [existing APVTS + smoothing infrastructure]
[Randomize Logic] ──depends on──> [existing APVTS parameter list]
```

### Dependency Notes

- **Randomize Logic requires existing APVTS:** The randomizer iterates `apvts.getParameter()` for each known parameter ID and calls `setValue()` (message thread) or equivalent. All 38 parameter IDs and ranges are already defined — no new parameter infrastructure needed.
- **Automatable trigger requires processBlock edge detection:** The trigger param must be polled in `processBlock`, and on rising edge, post a lambda to the message thread to execute randomization. Direct execution in processBlock is not safe (memory allocation in random number generation, APVTS writes).
- **UndoManager requires APVTS to be constructed with one:** If APVTS was not constructed with a UndoManager, adding it requires a constructor change. Check current PluginProcessor.h.
- **Per-group locks conflict with "randomize all" mental model:** Introducing locks changes UX expectations — must be clearly labeled. Not recommended for v1.2.

## MVP Definition

### Launch With (v1.2)

Minimum viable randomizer — what's needed to validate the concept and ship the milestone.

- [ ] GUI randomize button (dice icon or "RAND" label) — essential for discoverability; primary user-facing surface
- [ ] Randomize all parameter groups: tap positions (with minimum spacing constraint), tap levels, feedback sources and gains, filter cutoffs, multiplier, wet/dry — the feature only has value if it covers the full parameter space
- [ ] Clamped ranges for dangerous parameters: feedback gain max 0.7, wet/dry min 0.2 / max 0.9 — prevents oscillation runaway and "plugin disappeared" confusion
- [ ] Single undo transaction wrapping all 38 parameter changes — undo must work; this is table stakes
- [ ] Automatable trigger parameter — the primary differentiator; enables DAW-driven evolving randomization; described in milestone requirements

### Add After Validation (v1.x)

- [ ] Per-group lock toggles — add when users report wanting to preserve tap positions while randomizing feedback, or vice versa. Trigger: first user feedback after v1.2 ships.
- [ ] Musical grid snap for tap positions — snap random taps to subdivisions of the delay range. Trigger: if randomized results feel unmusical in practice testing.

### Future Consideration (v2+)

- [ ] Randomize amount / deviation slider — randomize within ±N% of current values; enables subtle variation. Defer: adds UI complexity and a "current baseline" state-tracking problem.
- [ ] Seed parameter for reproducible randomization — elegant for automation, but requires seeded RNG and seed state persistence. Defer: premature until users report wanting to reproduce specific random results.
- [ ] Smart randomization (harmony-aware, rhythm-aware) — constrain tap positions to musical intervals relative to tempo. Defer: requires significant DSP logic and tempo knowledge; may not be worth complexity for an experimental plugin.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| GUI randomize button | HIGH | LOW | P1 |
| Randomize all params (full coverage) | HIGH | LOW | P1 |
| Clamped dangerous parameter ranges | HIGH | LOW | P1 |
| Single undo transaction | HIGH | MEDIUM | P1 |
| Minimum inter-tap spacing constraint | HIGH | LOW | P1 |
| Automatable trigger parameter | HIGH | MEDIUM | P1 |
| Per-group lock toggles | MEDIUM | MEDIUM | P2 |
| Musical grid snap for tap positions | MEDIUM | MEDIUM | P2 |
| Randomize amount control | MEDIUM | HIGH | P3 |
| Seed-based reproducible randomization | MEDIUM | HIGH | P3 |

**Priority key:**
- P1: Must have for v1.2 launch
- P2: Add after validation
- P3: Future consideration (v2+)

## Competitor Feature Analysis

| Feature | Logic Pro Delay Designer | Eventide UltraTap | ValhallaUberMod | Our Approach |
|---------|--------------------------|-------------------|-----------------|--------------|
| Randomize button | Yes — random taps with configurable min/max tap count, timing range, pan range, level range | No direct randomize; "Slurm" adds random detuning/diffusion continuously | TAPS Random: continuous % control over tap spacing randomness (not a trigger) | One-shot trigger button + automatable trigger param |
| Tap position randomization | Random within time range with grid option | N/A (fixed tap algorithm) | Continuous spacing randomization as a parameter | Random within range + minimum spacing constraint |
| Feedback randomization | No | N/A | N/A | Full feedback matrix randomization (sources + gains) |
| Undo support | Host-managed | Host-managed | Host-managed | Single undo transaction via UndoManager |
| Automatable trigger | No | No | No | Yes — primary differentiator |
| Constrained ranges | Configurable min/max for each property | N/A | N/A | Hardcoded safe ranges for feedback + wet/dry |

## Sources

- [KVR Audio: Synths with random preset/parameter features](https://www.kvraudio.com/forum/viewtopic.php?t=565834) — community discussion of randomizer UX patterns; lock parameters, selective randomization expectations. MEDIUM confidence.
- [Vital forum: Patch Randomization feature request](https://forum.vital.audio/t/patch-randomization/9970) — user expectations for randomization in a complex synth. MEDIUM confidence.
- [HISE forum: Undo/Redo with Randomization](https://forum.hise.audio/topic/9383/undo-redo-with-randomization/7) — confirmed undo queue flooding as a known pitfall; transaction approach is the solution. HIGH confidence.
- [Cycling '74: Randomise all parameters without destroying undo queue](https://cycling74.com/forums/randomise-all-parameters-without-destroying-undo-queue) — same undo flooding problem, same solution. HIGH confidence.
- [ValhallaUberMod TAPS Parameters](https://valhalladsp.com/2012/01/26/valhallaubermod-the-taps-parameters/) — TAPS Random behavior; 0% = equal spacing, increasing = randomized spacing. HIGH confidence (official docs).
- [Steinberg VST3 developer portal: Parameters and Automation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Parameters+Automation/Index.html) — "no automatable parameter shall influence another automatable parameter." HIGH confidence (official spec).
- [Integra Audio: Top 12 Randomizer Plugins 2025](https://integraudio.com/12-best-randomizer-plugins/) — ecosystem survey; DEVIANCE pattern (amount control), Magic Dice (full chain randomizer), Cluster Delay randomize button behavior. MEDIUM confidence.
- Training data: Logic Pro Delay Designer random taps feature (configurable min/max count, timing, pan, level ranges). MEDIUM confidence.
- PROJECT.md v1.2 milestone requirements (HIGH confidence — direct source).

---
*Feature research for: preset randomizer, multi-tap delay plugin*
*Researched: 2026-03-10*
