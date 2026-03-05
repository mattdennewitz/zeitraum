---
phase: 02-core-delay-engine
plan: 01
subsystem: dsp
tags: [one-pole-smoother, analog-character, bbd, tpt-filter, parameter-smoothing, catch2]

# Dependency graph
requires:
  - phase: 01-project-scaffolding
    provides: JUCE plugin scaffold with CMake build, Catch2 test framework
provides:
  - OnePoleSmooth header-only JUCE-free parameter smoother
  - CharacterProcessor BBD-style analog character with HF roll-off and noise
  - Unit tests for both classes
affects: [02-core-delay-engine, 03-feedback-matrix, 05-gui]

# Tech tracking
tech-stack:
  added: []
  patterns: [one-pole exponential smoothing, TPT filter for modulation-safe HF roll-off, per-channel dual-mono DSP]

key-files:
  created:
    - src/dsp/OnePoleSmooth.h
    - src/dsp/CharacterProcessor.h
    - test/dsp/OnePoleSmoothTests.cpp
    - test/dsp/CharacterProcessorTests.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "OnePoleSmooth uses fixed-size array filter per channel (max 2) in CharacterProcessor for zero-allocation audio-thread safety"
  - "CharacterProcessor noise level at 0.0005 (-66dBFS) at full character for subtle BBD authenticity"

patterns-established:
  - "Header-only DSP in src/dsp/: JUCE-free when possible, header-only always"
  - "One-pole smoothing formula: alpha = 1 - exp(-2*pi / (timeMs * 0.001 * sampleRate))"
  - "Per-channel TPT filter array for dual-mono DSP processing"

requirements-completed: [GUI-04, INTG-04]

# Metrics
duration: 8min
completed: 2026-03-05
---

# Phase 02 Plan 01: DSP Helpers Summary

**One-pole exponential parameter smoother and BBD-style analog character processor with comprehensive unit tests**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-05T18:03:17Z
- **Completed:** 2026-03-05T18:11:33Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- OnePoleSmooth: JUCE-free header-only one-pole exponential smoother matching CLAUDE.md formula exactly
- CharacterProcessor: BBD-style analog character with TPT lowpass (20kHz-4kHz) and noise floor (0-0.0005) scaled by characterAmount
- 14 unit tests covering convergence, reset, isSmoothing, alpha accuracy, clean passthrough, HF attenuation, noise floor, and dual-mono independence

## Task Commits

Each task was committed atomically:

1. **Task 2: Update CMakeLists.txt for new test files** - `0d3d735` (chore)
2. **Task 1 RED: Failing tests for OnePoleSmooth and CharacterProcessor** - `4632866` (test)
3. **Task 1 GREEN: OnePoleSmooth and CharacterProcessor implementations** - `5658ae5` (feat)

_Note: Task 2 was executed first per plan NOTE (CMake must know about files before test compilation). Task 1 GREEN was committed alongside TapReader by external tooling._

## Files Created/Modified
- `src/dsp/OnePoleSmooth.h` - One-pole exponential parameter smoother (JUCE-free, header-only)
- `src/dsp/CharacterProcessor.h` - BBD-style analog character processor (FirstOrderTPTFilter + Random)
- `test/dsp/OnePoleSmoothTests.cpp` - 5 test cases: convergence, reset, isSmoothing, alpha formula, defaults
- `test/dsp/CharacterProcessorTests.cpp` - 4 test cases: clean passthrough, HF attenuation, noise floor, dual-mono
- `CMakeLists.txt` - Added new test source files to ZeitraumTests target

## Decisions Made
- Used fixed-size array (lpFilter[2]) instead of std::vector in CharacterProcessor for zero-allocation guarantee on audio thread
- processSample uses channel=0 internally since each per-channel filter is prepared with numChannels=1
- Noise seeded with default juce::Random (non-deterministic) for natural variation between instances
- Convergence threshold of 1e-6f for isSmoothing() matches practical audio precision needs

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed CharacterProcessor processSample channel index**
- **Found during:** Task 1 GREEN (implementation review)
- **Issue:** processSample(channel, sample) would pass channel=1 to a filter prepared with numChannels=1, causing out-of-bounds access
- **Fix:** Changed to processSample(0, sample) since each filter is per-channel and prepared with 1 channel
- **Files modified:** src/dsp/CharacterProcessor.h
- **Verification:** Dual-mono independence test passes, no out-of-bounds access
- **Committed in:** 5658ae5

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Essential for correctness -- would have caused undefined behavior on channel 1 processing.

## Issues Encountered
- External linter tooling created additional files (TapReader.h, DelayEngine.h, TapReaderTests.cpp, DelayEngineTests.cpp) from future plans during execution. These were managed by keeping them out of commits for this plan.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- OnePoleSmooth ready for use in TapReader, DelayEngine, and PluginProcessor parameter smoothing
- CharacterProcessor ready for integration into delay feedback path
- Both classes tested and verified at 44.1kHz and 48kHz sample rates

---
*Phase: 02-core-delay-engine*
*Completed: 2026-03-05*
