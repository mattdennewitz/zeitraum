# Requirements: Multi-Tap Delay

**Defined:** 2026-03-10
**Core Value:** The feedback routing matrix and per-tap control over a shared serial delay line

## v1.2 Requirements

Requirements for v1.2 Somewhat-Controlled Random Settings.

### Randomizer

- [x] **RAND-01**: Randomize button generates new random values for all sound-shaping parameters (tap positions, levels, feedback routing, filters, multiplier, wet/dry)
- [x] **RAND-02**: Randomized tap positions are sorted ascending (Tap 1 earliest in delay line)
- [x] **RAND-03**: Feedback gain sum is normalized to ~80% max to prevent transient instability
- [x] **RAND-04**: Wet/dry is clamped to [0.2, 0.9] range during randomization
- [x] **RAND-05**: Mode and trigger parameters (OUTPUT_MIX, TEMPO_SYNC, QUANTIZE, NOTE_DIV, RANDOMIZE) are excluded from randomization

### GUI

- [ ] **GUI-01**: Randomize button visible in plugin editor UI

### Automation

- [ ] **AUTO-01**: RANDOMIZE parameter is automatable in DAW automation lanes
- [ ] **AUTO-02**: Trigger uses rising-edge detection to prevent continuous re-randomization when automation holds value high
- [ ] **AUTO-03**: Session load does not trigger false randomization (prevTriggerState initialized from saved value)

## v1.1 Requirements (Complete)

### Feedback Matrix

- [x] **FB-01**: Feedback tap gain sliders (FB_TAP1_LVL-FB_TAP8_LVL) control the level of each tap's contribution to the feedback signal

## Future Requirements

### DSP Enhancements

- **DSP-01**: Doppler/tape artifacts when delay time is swept (pitch-shifting modulation character)
- **DSP-02**: Cross-channel feedback routing (L taps feed R input and vice versa)

### UX

- **UX-01**: Per-tap mute/solo for quick auditioning
- **UX-02**: Per-group lock toggles for randomizer (lock taps/levels/feedback/filters individually)
- **UX-03**: Randomize amount slider (±N% deviation from current values)

### Format

- **FMT-01**: CLAP format support via clap-juce-extensions

## Out of Scope

| Feature | Reason |
|---------|--------|
| Full delay engine rewrite | This is a targeted fix, not a redesign |
| Per-group lock toggles | Defer to future based on user feedback |
| Seed-based deterministic randomization | Adds state-tracking complexity |
| Randomize amount slider | Strong v2 candidate but not v1.2 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FB-01 | Phase 6 | Complete |
| RAND-01 | Phase 7 | Complete |
| RAND-02 | Phase 7 | Complete |
| RAND-03 | Phase 7 | Complete |
| RAND-04 | Phase 7 | Complete |
| RAND-05 | Phase 7 | Complete |
| GUI-01 | Phase 7 | Pending |
| AUTO-01 | Phase 8 | Pending |
| AUTO-02 | Phase 8 | Pending |
| AUTO-03 | Phase 8 | Pending |

**Coverage:**
- v1.2 requirements: 8 total
- Mapped to phases: 8
- Unmapped: 0

---
*Requirements defined: 2026-03-10*
*Last updated: 2026-03-10 after v1.2 roadmap creation*
