---
phase: 05-gui
plan: 01
subsystem: ui
tags: [juce, lookandfeel, apvts, flexbox, dark-theme]

# Dependency graph
requires:
  - phase: 04-daw-integration
    provides: "APVTS parameters, state persistence, createEditor entry point"
provides:
  - "ZeitraumLookAndFeel dark/teal theme for all components"
  - "TopBar component with all global controls and APVTS attachments"
  - "Resizable custom editor shell with proportional layout skeleton"
affects: [05-02, 05-03]

# Tech tracking
tech-stack:
  added: []
  patterns: [LookAndFeel_V4 ColourScheme theming, FlexBox top bar layout, attachToComponent label pattern]

key-files:
  created:
    - src/ui/ZeitraumLookAndFeel.h
    - src/ui/TopBar.h
  modified:
    - src/PluginEditor.h
    - src/PluginEditor.cpp
    - src/PluginProcessor.cpp

key-decisions:
  - "LookAndFeel declared as first editor member for correct destruction order"
  - "ComboBox items match exact APVTS AudioParameterChoice strings, not plan's suggested labels"
  - "Labels use attachToComponent for automatic positioning left of sliders"

patterns-established:
  - "LookAndFeel-first member ordering: declare LookAndFeel before all child components in editor"
  - "ComboBox-before-attachment: always populate items before creating ComboBoxAttachment"
  - "FlexBox row layout for horizontal control strips"

requirements-completed: [GUI-01]

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 5 Plan 1: Editor Shell and Theme Summary

**Custom dark/teal LookAndFeel with resizable 900x500 editor shell, top bar with 8 global controls (delay, multiplier, mix, character, quantize, sync, note division, output mix) all APVTS-attached**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T15:12:57Z
- **Completed:** 2026-03-08T15:15:01Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Custom LookAndFeel_V4 subclass with dark background and teal accent colour scheme
- TopBar component with 4 LinearBar sliders, 2 toggle buttons, 2 combo boxes all wired via APVTS attachments
- Resizable editor (700x400 to 1400x900) with proportional layout: top bar + left/right panel placeholders
- GenericAudioProcessorEditor fully replaced with custom ZeitraumEditor

## Task Commits

Each task was committed atomically:

1. **Task 1: Create LookAndFeel theme and TopBar component** - `e9d1a45` (feat)
2. **Task 2: Rebuild PluginEditor with layout skeleton and switch createEditor** - `705434f` (feat)

## Files Created/Modified
- `src/ui/ZeitraumLookAndFeel.h` - LookAndFeel_V4 subclass with dark/teal ColourScheme and custom draw overrides for sliders, toggles, combos, popup menus
- `src/ui/TopBar.h` - Horizontal strip of global controls with FlexBox layout and APVTS attachments
- `src/PluginEditor.h` - Custom editor with LookAndFeel, processorRef, TopBar members in correct destruction order
- `src/PluginEditor.cpp` - Resizable editor with paint placeholders and proportional resized() layout
- `src/PluginProcessor.cpp` - createEditor switched from GenericAudioProcessorEditor to ZeitraumEditor

## Decisions Made
- LookAndFeel declared as first member of editor class to ensure it outlives all child components (prevents use-after-free on close)
- ComboBox item strings matched to exact APVTS AudioParameterChoice values ("1/8 dot", "1/8 trip") rather than plan's suggested labels ("1/4 dotted", "1/8 triplet") -- prevents index mismatch
- Used attachToComponent for slider labels rather than manual FlexBox layout items, reducing FlexItem count

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] ComboBox items corrected to match parameter choices**
- **Found during:** Task 1 (TopBar creation)
- **Issue:** Plan specified NOTE_DIV items as "1/4", "1/8", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" but actual APVTS parameter has 6 choices: "1/4", "1/8", "1/8 dot", "1/8 trip", "1/16", "1/2"
- **Fix:** Used exact strings from createParameterLayout to prevent ComboBoxAttachment index mismatch
- **Files modified:** src/ui/TopBar.h
- **Verification:** Build succeeds, combo displays correct items
- **Committed in:** e9d1a45 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential fix to prevent parameter mismatch. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Editor shell ready for Plan 02 (tap column components) to fill left panel placeholder
- Editor shell ready for Plan 03 (feedback matrix editor) to fill right panel placeholder
- LookAndFeel theme inherited by all future child components automatically

---
*Phase: 05-gui*
*Completed: 2026-03-08*
