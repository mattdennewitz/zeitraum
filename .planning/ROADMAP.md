# Roadmap: Multi-Tap Delay

## Overview

Build a JUCE audio plugin that recreates the shared serial delay line architecture of the Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor. The journey goes from a compiling pass-through plugin, through the core 8-tap delay engine, to the feedback routing matrix that defines the product, then DAW integration (automation, tempo sync, state persistence), and finally the GUI. Each phase delivers a verifiable capability that the next phase builds on.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Project Scaffolding** - Compiling pass-through plugin with build automation and AU validation
- [ ] **Phase 2: Core Delay Engine** - Functional stereo 8-tap delay with per-tap controls, character filtering, and smooth modulation
- [ ] **Phase 3: Feedback Matrix** - Full feedback routing matrix with preset mixes and stability safeguards
- [ ] **Phase 4: DAW Integration** - Automation, tempo sync, and session state persistence
- [ ] **Phase 5: GUI** - Clean/modern interface with tap position display and feedback matrix editor

## Phase Details

### Phase 1: Project Scaffolding
**Goal**: A compiling plugin shell that builds as VST3 and AU, passes AU validation, and provides the Makefile-driven workflow for all subsequent development
**Depends on**: Nothing (first phase)
**Requirements**: INFR-01, INFR-02, INFR-03
**Success Criteria** (what must be TRUE):
  1. Running `make build` produces VST3 and AU plugin binaries without errors
  2. Running `make validate` passes `auval` on the pass-through plugin
  3. Loading the plugin in a DAW shows it in the plugin list and passes audio through unchanged
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- Build infrastructure, plugin source, tests, and CLAUDE.md
- [x] 01-02-PLAN.md -- AU validation and DAW verification checkpoint

### Phase 2: Core Delay Engine
**Goal**: Users hear a functional stereo 8-tap delay with independent tap positioning, per-tap levels, wet/dry mix, delay time multiplier, tap quantization, and analog character -- the sonic foundation of the product
**Depends on**: Phase 1
**Requirements**: CORE-01, CORE-02, CORE-03, CORE-04, CORE-05, CORE-06, CORE-07, CORE-08, CORE-09, MIX-01, INTG-04, GUI-04, INFR-04
**Success Criteria** (what must be TRUE):
  1. Plugin processes stereo audio through a shared delay line with 8 independently audible taps
  2. Adjusting per-tap level, position, base delay time, and multiplier produces smooth changes with no clicks or zipper noise
  3. Tap times can be quantized to 10ms steps and tap presets (including equal-spacing default) can be saved and recalled
  4. Delay path exhibits audible analog character (HF roll-off, subtle warmth) compared to a clean bypass
  5. Plugin runs glitch-free at 64-sample buffer sizes at 44.1kHz and 96kHz sample rates
**Plans**: 4 plans

Plans:
- [ ] 02-01-PLAN.md -- OnePoleSmooth and CharacterProcessor DSP helpers with unit tests
- [ ] 02-02-PLAN.md -- TapReader and DelayEngine DSP classes with unit tests
- [ ] 02-03-PLAN.md -- Parameter layout, processor wiring, and integration tests
- [ ] 02-04-PLAN.md -- Tap presets, AU validation, and DAW listening checkpoint

### Phase 3: Feedback Matrix
**Goal**: Users can route any combination of tap outputs and preset mixes back into the delay input with independent gain, producing the complex evolving textures that define this plugin
**Depends on**: Phase 2
**Requirements**: FDBK-01, FDBK-02, FDBK-03, FDBK-04, MIX-02, MIX-03
**Success Criteria** (what must be TRUE):
  1. User can set independent feedback gain from any tap or preset mix (odd, even, rising, falling) back to the delay input
  2. Cranking feedback gains to maximum produces saturation and sustain but never runaway oscillation or digital blowup
  3. Feedback path filtering (highpass/lowpass) audibly shapes the tone of repeated echoes
  4. Feedback routing state is visible in an interactive matrix display
**Plans**: 4 plans

Plans:
- [ ] 03-01-PLAN.md -- FeedbackFilter and FeedbackSaturator DSP classes with unit tests
- [ ] 03-02-PLAN.md -- FeedbackMatrix DSP class with unit tests
- [ ] 03-03-PLAN.md -- DelayEngine feedback integration and parameter wiring
- [ ] 03-04-PLAN.md -- Output mix presets, AU validation, and DAW listening checkpoint

### Phase 4: DAW Integration
**Goal**: Plugin behaves as a first-class DAW citizen with full automation, tempo sync, and reliable session recall
**Depends on**: Phase 3
**Requirements**: INTG-01, INTG-02, INTG-03
**Success Criteria** (what must be TRUE):
  1. Every parameter (tap levels, positions, feedback gains, filters, mix) is visible and recordable in DAW automation lanes
  2. Enabling tempo sync locks delay times to host BPM with selectable note divisions (1/4, 1/8, dotted, triplet)
  3. Saving and reopening a DAW session restores all plugin state exactly -- including tap presets and feedback matrix settings
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

### Phase 5: GUI
**Goal**: Users interact with a clean, modern interface that makes the 8-tap delay and feedback matrix intuitive to use
**Depends on**: Phase 4
**Requirements**: GUI-01, GUI-02, GUI-03
**Success Criteria** (what must be TRUE):
  1. Plugin window displays a clean/modern DAW-style interface (not skeuomorphic hardware imitation)
  2. Tap positions and timing are visually displayed and can be adjusted by dragging
  3. Per-tap level controls are visible and adjustable directly in the GUI
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Project Scaffolding | 2/2 | Complete | 2026-03-05 |
| 2. Core Delay Engine | 0/4 | Not started | - |
| 3. Feedback Matrix | 4/4 | Complete | 2026-03-06 |
| 4. DAW Integration | 0/1 | Not started | - |
| 5. GUI | 0/1 | Not started | - |
