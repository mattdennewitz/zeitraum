---
phase: 02-core-delay-engine
plan: 04
subsystem: dsp
tags: [delay, presets, valuetree, smoothing, au-validation, glitch-free]

requires:
  - phase: 02-core-delay-engine/03
    provides: "Full processor integration with delay engine, parameters, and state"
provides:
  - "Tap preset save/recall via ValueTree in APVTS state"
  - "Glitch-free delay time sweeps with proper per-sample smoother separation"
  - "AU-validated Phase 2 delay engine"
  - "Sweep glitch detection tests at buffer size 64"
affects: [03-feedback-matrix, 05-gui]

tech-stack:
  added: []
  patterns:
    - "Pre-compute smoothed values into scratch buffers before channel loop to avoid double-advance"
    - "50ms smoothing for delay-time parameters, 5ms for gain crossfades"
    - "ValueTree child TapPresets for named preset storage inside APVTS state"
    - "Runtime scratch buffer resize in process() for block size safety"

key-files:
  created: []
  modified:
    - src/PluginProcessor.h
    - src/PluginProcessor.cpp
    - src/dsp/DelayEngine.h
    - src/dsp/TapReader.h
    - test/PluginTests.cpp
    - test/dsp/DelayEngineTests.cpp

key-decisions:
  - "Tap presets stored as ValueTree children -- auto-persist via existing XML state mechanism"
  - "Default equal-spacing is parameter defaults, not a named preset (CORE-06)"
  - "50ms smoothing for base delay, multiplier, and tap position to prevent sweep glitches"
  - "Pre-allocated scratch buffers for smoothed values to avoid audio-thread allocation"

patterns-established:
  - "Smoother separation: pre-compute per-sample values before channel loop to prevent multi-channel double-advance"
  - "Scratch buffer pattern: allocate in prepare(), resize safety check in process(), index by sample"
  - "Glitch detection testing: sweep parameters at small buffer sizes, measure maxDelta between consecutive output samples"

requirements-completed: [CORE-06, CORE-08, CORE-09]

duration: 45min
completed: 2026-03-05
---

# Phase 2 Plan 4: Tap Presets and Phase Gate Summary

**Tap preset save/recall with ValueTree persistence, glitch-free delay sweeps via smoother separation and 50ms delay-time smoothing, verified with sweep detection tests at 64-sample buffers**

## Performance

- **Duration:** 45 min (includes glitch investigation and fix iteration)
- **Started:** 2026-03-05T18:32:19Z
- **Completed:** 2026-03-05T19:51:59Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Tap preset save/recall/list methods on ZeitraumProcessor with ValueTree persistence
- Fixed delay sweep glitches: smoothers were advancing twice per sample (once per channel)
- Increased delay-time smoothing from 10ms to 50ms for click-free parameter sweeps
- Added runtime scratch buffer safety to handle block size changes
- Comprehensive sweep glitch detection tests at 64-sample buffer size (maxDelta measured at 0.032-0.079, well below 0.5 click threshold)
- AU validation passes with all Phase 2 parameters

## Task Commits

Each task was committed atomically:

1. **Task 1: Tap preset save/recall and integration tests** - `15dfcb0` (feat)
2. **Task 2a: Smoother separation fix** - `396f148` (fix)
3. **Task 2b: Buffer safety and glitch detection tests** - `facb2ad` (fix)

## Files Created/Modified
- `src/PluginProcessor.h` - Added saveTapPreset, recallTapPreset, getTapPresetNames declarations
- `src/PluginProcessor.cpp` - Implemented tap preset save/recall/list with ValueTree storage
- `src/dsp/DelayEngine.h` - Pre-compute smoothed values into scratch buffers before channel loop; increased smoothing to 50ms; added runtime buffer resize safety
- `src/dsp/TapReader.h` - Increased tap position smoothing from 10ms to 50ms
- `test/PluginTests.cpp` - Added 3 preset tests and 1 processor-level sweep glitch detection test
- `test/dsp/DelayEngineTests.cpp` - Added 2 DSP-level sweep glitch detection tests (base delay and tap position sweeps)

## Decisions Made
- Tap presets stored as ValueTree children under "TapPresets" node -- persists automatically through existing XML state mechanism, no changes to getStateInformation/setStateInformation needed
- Default equal-spacing handled by parameter defaults (TAP{i}_POS = i/8), not stored as a named preset
- 50ms smoothing time for delay-time parameters balances responsiveness with glitch-free sweeps
- Scratch buffers resized at runtime in process() as safety measure against block size changes

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed smoothers advancing twice per sample in stereo processing**
- **Found during:** Task 2 (DAW listening test -- user reported glitches)
- **Issue:** baseDelaySmoother, multiplierSmoother, characterSmoother, and TapReader position/level smoothers all called getNextValue() inside the per-channel loop, causing them to advance twice per sample for stereo. This made smoothing effectively 2x faster than intended and created inconsistent delay times between L/R channels.
- **Fix:** Pre-compute all smoothed values into scratch buffers (allocated in prepare()) in a single pass before the channel loop. Both channels now read identical smoothed values per sample.
- **Files modified:** src/dsp/DelayEngine.h
- **Verification:** make test passes, AU validation passes, sweep glitch detection tests confirm maxDelta < 0.08
- **Committed in:** 396f148

**2. [Rule 1 - Bug] Increased delay-time smoothing from 10ms to 50ms**
- **Found during:** Task 2 (DAW listening test)
- **Issue:** 10ms smoothing time was too aggressive for delay-time parameters, causing rapid read-pointer movement that could produce audible pitch artifacts during Base Delay and Tap Position sweeps.
- **Fix:** Increased base delay, multiplier, and tap position smoothing to 50ms. Character and level smoothing kept shorter (10ms/5ms) as they don't cause discontinuities.
- **Files modified:** src/dsp/DelayEngine.h, src/dsp/TapReader.h
- **Committed in:** 396f148

**3. [Rule 1 - Bug] Added runtime scratch buffer resize safety**
- **Found during:** Task 2 (glitch test development)
- **Issue:** Process() could receive blocks larger than maxBlockSize passed to prepare(), causing buffer overflow crash. Discovered when test warmup used 512-sample blocks after preparing with 64.
- **Fix:** Added runtime size check and resize at start of process() to handle any block size.
- **Files modified:** src/dsp/DelayEngine.h
- **Committed in:** facb2ad

---

**Total deviations:** 3 auto-fixed (3 bugs)
**Impact on plan:** All fixes necessary for correct, glitch-free audio output. No scope creep.

## Issues Encountered
- JUCE assertion in juce_FirstOrderTPTFilter.cpp:55 during AU validation at extreme sample rates -- cosmetic only, does not affect validation PASS result
- JUCE Timer/Singleton leak assertions in test teardown -- cosmetic, expected in headless test environment

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 complete: all core delay engine requirements addressed (CORE-01 through CORE-09, MIX-01, INTG-04, GUI-04, INFR-04)
- Delay engine ready for Phase 3 feedback matrix integration
- Tap preset system ready for Phase 5 GUI preset selector
- Note: Phase 3 feedback matrix will need careful gain staging to prevent runaway feedback

---
*Phase: 02-core-delay-engine*
*Completed: 2026-03-05*
