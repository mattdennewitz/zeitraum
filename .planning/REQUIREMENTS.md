# Requirements: Multi-Tap Delay

**Defined:** 2026-03-05
**Core Value:** The feedback routing matrix and per-tap control over a shared serial delay line — creating complex, evolving delay textures

## v1 Requirements

### Delay Core

- [x] **CORE-01**: Plugin provides 8 delay taps along a shared serial delay line (stereo, one line per channel)
- [x] **CORE-02**: User can set delay time with base range ~10–150ms
- [x] **CORE-03**: User can scale all tap times proportionally via a multiplier dial (total up to ~5s)
- [x] **CORE-04**: User can adjust individual level for each of the 8 taps
- [x] **CORE-05**: User can position each tap freely along the delay line (not fixed equal spacing)
- [x] **CORE-06**: Equal spacing is provided as the default tap preset
- [x] **CORE-07**: Tap times can be quantized to 10ms increments
- [x] **CORE-08**: Tap time presets can be saved and recalled
- [x] **CORE-09**: User can control wet/dry mix globally

### Feedback & Routing

- [x] **FDBK-01**: Plugin provides a feedback routing matrix where any tap or preset mix can be routed back to the delay input with independent gain
- [x] **FDBK-02**: Feedback matrix includes saturation/limiting to prevent runaway oscillation
- [x] **FDBK-03**: Feedback path includes highpass and lowpass filters for tonal shaping
- [x] **FDBK-04**: Plugin provides an interactive visual display of the feedback routing matrix

### Mixing & Output

- [x] **MIX-01**: Plugin operates in stereo (stereo input, stereo output)
- [ ] **MIX-02**: Plugin provides preset mixes: odd taps, even taps, rising-level, falling-level
- [x] **MIX-03**: Preset mixes are available as feedback sources in the routing matrix

### Integration

- [ ] **INTG-01**: All parameters are DAW-automatable
- [ ] **INTG-02**: Tempo sync is available (on/off toggle) with host BPM and note divisions (1/4, 1/8, dotted, triplet)
- [ ] **INTG-03**: Plugin state (all parameters + tap presets) saves and restores with DAW session
- [x] **INTG-04**: Analog character approximation: bandwidth limiting, mild noise, gentle HF roll-off in delay path

### Plugin Infrastructure

- [x] **INFR-01**: Plugin builds as VST3 and AU formats
- [x] **INFR-02**: Project uses CMake + Ninja with a Makefile automating all build tasks (following three-sisters pattern)
- [x] **INFR-03**: AU validation passes via `auval`
- [x] **INFR-04**: Plugin runs glitch-free at standard buffer sizes (64–512 samples)

### GUI

- [ ] **GUI-01**: Clean/modern DAW-style GUI (not skeuomorphic)
- [ ] **GUI-02**: Visual display of tap positions and timing
- [ ] **GUI-03**: Per-tap level controls visible and adjustable in GUI
- [x] **GUI-04**: Smooth parameter changes throughout (no clicks or zipper noise)

## v2 Requirements

### Enhanced DSP

- **DSP-01**: Doppler/tape artifacts when delay time is swept (pitch-shifting modulation character)
- **DSP-02**: Cross-channel feedback routing (L taps feed R input and vice versa)

### Enhanced UI

- **UI-01**: Per-tap mute/solo for quick auditioning

### Additional Formats

- **FMT-01**: CLAP format support via clap-juce-extensions

## Out of Scope

| Feature | Reason |
|---------|--------|
| Independent delay buffers per tap | Defeats the shared serial delay line architecture — the core identity |
| Granular/spectral processing | Different product category; scope creep |
| Built-in reverb | Dense multi-tap patterns already approach reverb-like textures naturally |
| Pattern/step sequencer | EchoBoy/UltraTap own this space; preset mixes + free positioning covers the need |
| Skeuomorphic hardware GUI | Clean/modern DAW aesthetic decided during questioning |
| MIDI note input / pitched delay | Different product (Karplus-Strong); comb effects emerge naturally from short delays |
| Modulation matrix (LFO/envelope/XY) | FabFilter Timeless owns this; DAW automation covers complex modulation |
| Sidechain ducking | Not core; users can chain a separate ducker plugin |
| Oversampling | Delay effects don't typically alias; unnecessary CPU cost |
| Multi-band processing | Scope creep; single full-bandwidth path with filtering |
| Standalone application | Plugin formats only |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORE-01 | Phase 2 | Complete |
| CORE-02 | Phase 2 | Complete |
| CORE-03 | Phase 2 | Complete |
| CORE-04 | Phase 2 | Complete |
| CORE-05 | Phase 2 | Complete |
| CORE-06 | Phase 2 | Complete |
| CORE-07 | Phase 2 | Complete |
| CORE-08 | Phase 2 | Complete |
| CORE-09 | Phase 2 | Complete |
| FDBK-01 | Phase 3 | Complete |
| FDBK-02 | Phase 3 | Complete |
| FDBK-03 | Phase 3 | Complete |
| FDBK-04 | Phase 3 | Complete |
| MIX-01 | Phase 2 | Complete |
| MIX-02 | Phase 3 | Pending |
| MIX-03 | Phase 3 | Complete |
| INTG-01 | Phase 4 | Pending |
| INTG-02 | Phase 4 | Pending |
| INTG-03 | Phase 4 | Pending |
| INTG-04 | Phase 2 | Complete |
| INFR-01 | Phase 1 | Complete |
| INFR-02 | Phase 1 | Complete |
| INFR-03 | Phase 1 | Complete |
| INFR-04 | Phase 2 | Complete |
| GUI-01 | Phase 5 | Pending |
| GUI-02 | Phase 5 | Pending |
| GUI-03 | Phase 5 | Pending |
| GUI-04 | Phase 2 | Complete |

**Coverage:**
- v1 requirements: 28 total
- Mapped to phases: 28
- Unmapped: 0

---
*Requirements defined: 2026-03-05*
*Last updated: 2026-03-05 after roadmap creation*
