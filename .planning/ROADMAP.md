# Roadmap: Zeitraum

## Overview

Build a JUCE audio plugin that recreates the time-domain processing architecture of the Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor. Stereo 8-tap delay with feedback routing matrix, per-tap controls, and custom GUI.

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-08)
- v1.1 Feedback Fixes -- Phase 6 (in progress)

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

### v1.1 Feedback Fixes

- [ ] **Phase 6: Feedback Tap Gain Fix** - Fix FeedbackGainCell value scaling so tap gain sliders send correct 0-100 values to parameters

## Phase Details

### Phase 6: Feedback Tap Gain Fix
**Goal**: Users can control individual tap contributions to the feedback signal via the matrix editor sliders
**Depends on**: Phase 5 (v1.0 complete)
**Requirements**: FB-01
**Success Criteria** (what must be TRUE):
  1. Moving a feedback tap gain slider (FB_TAP1_LVL through FB_TAP8_LVL) in the matrix editor audibly changes that tap's contribution to the feedback signal
  2. Setting a feedback tap gain slider to zero silences that tap's feedback contribution while other taps with non-zero gain continue feeding back
  3. Feedback tap gain values persist correctly through save/restore cycles (DAW session recall)
**Plans**: 1 plan
Plans:
- [ ] 06-01-PLAN.md -- Fix FeedbackGainCell value scaling and add regression test

## Progress

**Execution Order:**
Phase 6

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Project Scaffolding | v1.0 | 2/2 | Complete | 2026-03-05 |
| 2. Core Delay Engine | v1.0 | 4/4 | Complete | 2026-03-06 |
| 3. Feedback Matrix | v1.0 | 4/4 | Complete | 2026-03-06 |
| 4. DAW Integration | v1.0 | 2/2 | Complete | 2026-03-07 |
| 5. GUI | v1.0 | 3/3 | Complete | 2026-03-08 |
| 6. Feedback Tap Gain Fix | v1.1 | 0/1 | In progress | - |
