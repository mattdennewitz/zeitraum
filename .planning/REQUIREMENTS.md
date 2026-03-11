# Requirements: Multi-Tap Delay

**Defined:** 2026-03-10
**Core Value:** The feedback routing matrix and per-tap control over a shared serial delay line

## v1.1 Requirements

Requirements for v1.1 Feedback Fixes. Bug fix milestone.

### Feedback Matrix

- [ ] **FB-01**: Feedback tap gain sliders (FB_TAP1_LVL–FB_TAP8_LVL) control the level of each tap's contribution to the feedback signal

## Future Requirements

### DSP Enhancements

- **DSP-01**: Doppler/tape artifacts when delay time is swept (pitch-shifting modulation character)
- **DSP-02**: Cross-channel feedback routing (L taps feed R input and vice versa)

### UX

- **UX-01**: Per-tap mute/solo for quick auditioning

### Format

- **FMT-01**: CLAP format support via clap-juce-extensions

## Out of Scope

| Feature | Reason |
|---------|--------|
| Full delay engine rewrite | This is a targeted fix, not a redesign |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FB-01 | Phase 6 | Pending |

**Coverage:**
- v1.1 requirements: 1 total
- Mapped to phases: 1
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-10*
*Last updated: 2026-03-10 after initial definition*
