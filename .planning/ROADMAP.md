# Roadmap: Zeitraum

## Overview

Build a JUCE audio plugin that recreates the time-domain processing architecture of the Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor. Stereo 8-tap delay with feedback routing matrix, per-tap controls, and custom GUI.

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-08)
- v1.1 Feedback Fixes -- Phase 6 (shipped 2026-03-10)
- v1.2 Somewhat-Controlled Random Settings -- Phases 7-8 (in progress)

## Phases

<details>
<summary>v1.0 MVP (Phases 1-5) -- SHIPPED 2026-03-08</summary>

- [x] Phase 1: Project Scaffolding (2/2 plans) -- completed 2026-03-05
- [x] Phase 2: Core Delay Engine (4/4 plans) -- completed 2026-03-06
- [x] Phase 3: Feedback Matrix (4/4 plans) -- completed 2026-03-06
- [x] Phase 4: DAW Integration (2/2 plans) -- completed 2026-03-07
- [x] Phase 5: GUI (3/3 plans) -- completed 2026-03-08

Full details: .planning/milestones/v1.0-ROADMAP.md

</details>

<details>
<summary>v1.1 Feedback Fixes (Phase 6) -- SHIPPED 2026-03-10</summary>

- [x] Phase 6: Feedback Tap Gain Fix (1/1 plans) -- completed 2026-03-10

</details>

### v1.2 Somewhat-Controlled Random Settings

- [ ] **Phase 7: Randomizer Engine** - Implement parameter randomization with safety constraints and GUI button
- [ ] **Phase 8: Automation Trigger** - Automatable RANDOMIZE parameter with edge detection for DAW-driven randomization

## Phase Details

### Phase 7: Randomizer Engine
**Goal**: Users can randomize all sound-shaping parameters with a single button click, producing musically useful results without feedback instability
**Depends on**: Phase 6 (v1.1 complete)
**Requirements**: RAND-01, RAND-02, RAND-03, RAND-04, RAND-05, GUI-01
**Success Criteria** (what must be TRUE):
  1. Clicking the randomize button in the plugin GUI produces an audibly different delay configuration every time
  2. After randomization, tap positions are always ordered ascending (Tap 1 is earliest, Tap 8 is latest in the delay line)
  3. After randomization, the plugin does not produce runaway feedback oscillation or self-destruct -- feedback gains are bounded and wet/dry stays in a usable range
  4. Mode and trigger parameters (OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, NOTE_DIV, RANDOMIZE) remain unchanged after randomization
  5. All randomized parameter values are reflected in the GUI controls (sliders, bars, cells update automatically) and persist through save/restore
**Plans**: TBD

### Phase 8: Automation Trigger
**Goal**: Users can trigger randomization from DAW automation lanes, enabling evolving delay textures during playback
**Depends on**: Phase 7
**Requirements**: AUTO-01, AUTO-02, AUTO-03
**Success Criteria** (what must be TRUE):
  1. RANDOMIZE parameter appears in the DAW's automation lane list and can be drawn/recorded like any other parameter
  2. A rising edge (0-to-1 transition) in the RANDOMIZE automation lane triggers exactly one randomization -- holding the value high does not cause continuous re-randomization
  3. Loading a saved session where RANDOMIZE was saved as 1.0 does not trigger a false randomization on load
**Plans**: TBD

## Progress

**Execution Order:**
Phase 7 -> Phase 8

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Project Scaffolding | v1.0 | 2/2 | Complete | 2026-03-05 |
| 2. Core Delay Engine | v1.0 | 4/4 | Complete | 2026-03-06 |
| 3. Feedback Matrix | v1.0 | 4/4 | Complete | 2026-03-06 |
| 4. DAW Integration | v1.0 | 2/2 | Complete | 2026-03-07 |
| 5. GUI | v1.0 | 3/3 | Complete | 2026-03-08 |
| 6. Feedback Tap Gain Fix | v1.1 | 1/1 | Complete | 2026-03-10 |
| 7. Randomizer Engine | v1.2 | 0/? | Not started | - |
| 8. Automation Trigger | v1.2 | 0/? | Not started | - |
