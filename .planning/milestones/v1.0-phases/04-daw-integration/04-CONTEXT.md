# Phase 4: DAW Integration - Context

**Gathered:** 2026-03-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Plugin behaves as a first-class DAW citizen with full parameter automation, tempo-synced delay times, and reliable session state persistence. No new DSP capabilities or GUI work -- purely integration quality.

</domain>

<decisions>
## Implementation Decisions

### Tempo Sync Behavior
- Tempo sync replaces the base delay time with a musical note division (e.g. 1/8 note at 120 BPM = 250ms)
- Multiplier knob stays active in sync mode -- scales the synced base time for creative range beyond standard note values
- Tap positions continue to work as ratios of the (synced) base delay, same as free-running mode
- Minimal note division set: 1/4, 1/8, dotted 1/8, triplet 1/8 (~6 options)
- When DAW tempo changes mid-playback, delay time smoothly crossfades to new value (consistent with existing OnePoleSmooth architecture)
- New APVTS parameters: TEMPO_SYNC (bool toggle), NOTE_DIV (choice parameter)

### Automation UX
- All parameters automatable -- no exceptions, including OUTPUT_MIX and QUANTIZE
- Parameters organized using AudioProcessorParameterGroup: "Tap 1 > Position", "Tap 1 > Level", "Feedback > Tap 1", "Feedback > HP Freq", etc.
- Refactor flat parameter layout into grouped layout
- Parameter display shows formatted values with units: "80.0 ms", "50%", "440 Hz", "1/8 note"
- Improve valueToText lambdas where current formatting is insufficient

### State Persistence
- All new parameters (TEMPO_SYNC, NOTE_DIV) saved/restored via APVTS automatically
- Silent defaults for missing parameters when loading older sessions -- no user notification
- Bump pluginVersion from 2 to 3 in getStateInformation
- No migration code needed -- APVTS handles missing params gracefully with defaults

### Claude's Discretion
- Exact parameter group hierarchy and naming
- How to handle host BPM not available (fallback to 120 BPM or disable sync)
- Smoother time constant for tempo changes
- Whether to add 1/16 and 1/2 note divisions beyond the minimal set

</decisions>

<specifics>
## Specific Ideas

- Tempo sync should feel natural -- the base delay knob becomes a note selector when sync is on, everything else stays the same
- Smooth crossfade on tempo changes avoids clicks, consistent with the existing parameter smoothing approach throughout the plugin

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `OnePoleSmooth`: Already used for base delay, multiplier, character smoothing -- reuse for tempo-synced delay transitions
- `APVTS` with `ParameterID` versioning: All params use version 1, ready for forward compatibility
- `getStateInformation`/`setStateInformation`: Already implemented with pluginVersion attribute and null/corrupt XML guards

### Established Patterns
- Parameters cached via `getRawParameterValue` in constructor, read with `.load()` in processBlock
- Parameter smoothing: 50ms for delay-time params, 10ms for character -- same approach applies to synced delay times
- State stored as XML via APVTS `copyState`/`replaceState` with version attribute

### Integration Points
- `PluginProcessor::createParameterLayout()`: Flat layout needs refactoring into AudioProcessorParameterGroup hierarchy
- `PluginProcessor::processBlock()`: Needs to read host BPM from `getPlayHead()->getPosition()` and compute synced delay time
- `DelayEngine::process()`: Receives baseDelayMs -- tempo sync converts note division to ms before passing in, no engine changes needed
- `PluginProcessor` constructor: New cached param pointers for TEMPO_SYNC and NOTE_DIV

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 04-daw-integration*
*Context gathered: 2026-03-06*
