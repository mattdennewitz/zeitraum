# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — MVP

**Shipped:** 2026-03-08
**Phases:** 5 | **Plans:** 15 | **Execution time:** 2.01 hours

### What Was Built
- Stereo 8-tap delay engine with shared serial delay line and per-sample parameter smoothing
- 12-source feedback routing matrix with tanh saturation, RMS energy limiting, and HP/LP filtering
- Full DAW integration: 38 automatable parameters, tempo sync, XML state persistence (v3)
- Custom dark/teal GUI with draggable tap position bars, level faders, and feedback matrix editor
- Comprehensive test suite: 1,810 assertions across DSP unit tests and processor integration tests

### What Worked
- Header-only DSP classes kept JUCE-free when possible — easier unit testing, faster iteration
- Pre-computing smoothed values into scratch buffers before the channel loop avoided the multi-channel double-advance bug
- TDD caught the delay sweep glitch early (smoothers advancing twice per sample per channel)
- Phases built cleanly on each other — backward-compatible process() overload in Phase 3 preserved all Phase 2 tests
- AU validation passed on first attempt in Phase 1 scaffold — no retrofitting needed

### What Was Inefficient
- ROADMAP checkboxes were left unchecked across phases 2-5 — had to bulk-fix at milestone completion
- OUTPUT_MIX apply-and-reset pattern initially used string construction on the audio thread — caught in audit
- Phase 2 delay-time smoothing was initially 10ms (too fast), had to increase to 50ms to fix sweep glitches

### Patterns Established
- OnePoleSmooth formula: `alpha = 1 - exp(-2*pi / (timeMs * 0.001 * sampleRate))` — standard for all smoothing
- Smoothing times: 50ms delay-time, 10ms character, 7ms feedback gains, 5ms level/gain
- ParameterAttachment (not SliderAttachment) for custom drag components — proper gesture begin/set/end
- LookAndFeel declared as first editor member for correct destruction order
- Pop-before-push in delay engine inner loop for correct feedback timing

### Key Lessons
1. Pre-compute smoothed parameters into scratch buffers when processing multiple channels — avoids double-advance
2. Cache RangedAudioParameter* pointers in the constructor if you need setValueNotifyingHost on the audio thread — never construct strings there
3. ComboBox items must match exact AudioParameterChoice strings, not what the plan suggests
4. 50ms is the sweet spot for delay-time smoothing — fast enough to feel responsive, slow enough to avoid clicks at buffer size 64

### Cost Observations
- Model mix: quality profile throughout (opus for orchestration, sonnet for agents)
- Total execution: ~2 hours across 4 days
- Notable: Phase 2 (core DSP) took 72min — most complex phase. Phases 1, 4, 5 averaged 8min each.

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Execution Time | Phases | Key Change |
|-----------|---------------|--------|------------|
| v1.0 | 2.01 hours | 5 | Initial milestone — established all patterns |

### Cumulative Quality

| Milestone | Tests | Assertions | Header-Only DSP |
|-----------|-------|------------|-----------------|
| v1.0 | 30+ cases | 1,810 | 6 classes |

### Top Lessons (Verified Across Milestones)

1. Header-only, JUCE-free DSP classes enable fast unit testing without plugin lifecycle overhead
2. Pre-compute smoothed values into scratch buffers before multi-channel processing loops
