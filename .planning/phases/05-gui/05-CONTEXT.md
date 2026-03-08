# Phase 5: GUI - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Build a clean, modern interface for the Zeitraum plugin that makes the 8-tap delay and feedback matrix intuitive to use. Covers GUI-01 (clean/modern DAW-style), GUI-02 (tap position display), and GUI-03 (per-tap level controls). No new DSP, no new parameters — purely visual/interaction layer over existing APVTS parameters.

</domain>

<decisions>
## Implementation Decisions

### Tap Position Display
- Vertical bar graph style — 8 columns, one per tap
- Bar height represents tap position (delay time along the shared delay line)
- Drag the top edge of each bar to reposition taps
- Level faders directly below each position bar (stacked column per tap)
- Millisecond labels on each bar + grid lines when quantize is enabled
- Taps differentiated by number labels (1-8), all same accent color

### Overall Layout
- Window size: 900×500, resizable with minimum size (~700×400)
- Top bar: horizontal strip with linear sliders for global controls (base delay, multiplier, mix, character, quantize toggle, tempo sync toggle, note division, output mix preset selector)
- No plugin branding in the GUI — DAW shows name in window title
- Main content: left side (~60%) = tap position bars + level faders, right side (~40%) = feedback matrix
- Feedback filter controls (HP/LP freq sliders, on/off toggles) below the feedback matrix in the right panel

### Feedback Matrix Editor
- Vertical list of gain cells — one row per feedback source
- Sources: 8 taps, then 4 preset mixes (Odd, Even, Rising, Falling)
- Section labels: "Taps" header above tap rows, "Mixes" header above preset mix rows
- Each cell is a horizontal fill bar showing gain percentage
- Click and drag horizontally to set gain
- Double-click any cell to type a precise value
- Static color fill (no signal metering/glow)

### Visual Identity
- Dark theme (dark background, matching current #2D2D2D base)
- Teal/cyan accent color for active elements (bars, faders, gain fills)
- Inactive/zero-gain elements dimmed (lower opacity) but always visible
- Single accent color + numbered labels to differentiate taps (not color-coded per tap)

### Parameter Value Display
- Always-visible value labels on all controls (ms, %, dB as appropriate)
- Double-click-to-type available on all controls (sliders, bars, faders, matrix cells)

### Tap Presets
- Named presets (user types a name when saving)
- Dropdown + save button — placement at Claude's discretion (top bar or above tap bars)

### Claude's Discretion
- Exact spacing, margins, and typography
- Tap preset UI placement (top bar vs above tap bars)
- Loading/empty states
- Exact minimum window dimensions
- Font choices and sizes
- Scroll behavior if window is at minimum size

</decisions>

<specifics>
## Specific Ideas

- Bar graph style inspired by mixing console — position bars stacked above level faders per channel
- Grid lines for quantization steps (10ms increments when quantize is on)
- "Clean, modern, DAW-style" not skeuomorphic — think Ableton/Bitwig aesthetic over hardware imitation

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- ZeitraumEditor: bare shell at 400×300 with dark background (#2D2D2D) — will be completely replaced
- ZeitraumProcessor::apvts: full APVTS with all parameters already wired
- Cached parameter pointers in PluginProcessor.h: tapPosParams[8], tapLevelParams[8], fbTapGainParams[8], fbMixGainParams[4], fbHPFreqParam, fbLPFreqParam, fbHPOnParam, fbLPOnParam, outputMixParam, tempoSyncParam, noteDivParam, baseDelayParam, multiplierParam, mixParam, characterParam, quantizeParam

### Established Patterns
- `DONT_SET_USING_JUCE_NAMESPACE=1` — all JUCE types require `juce::` prefix
- Header-only DSP classes in `src/dsp/`
- `src/ui/` directory exists (empty, with .gitkeep) — ready for custom components and LookAndFeel
- Parameter access via `getRawParameterValue` cached pointers with `.load()` in audio thread

### Integration Points
- ZeitraumEditor::resized() and paint() — main entry point for GUI
- ZeitraumProcessor& processorRef — editor has reference to processor (and thus apvts)
- juce::AudioProcessorValueTreeState::SliderAttachment / ButtonAttachment for parameter binding
- saveTapPreset() / recallTapPreset() / getTapPresetNames() already on processor

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-gui*
*Context gathered: 2026-03-08*
