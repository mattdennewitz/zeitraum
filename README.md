# Zeitraum

A multi-tap delay plugin inspired by the Verbos Multi-Delay Processor and Buchla 288 Time Domain Processor. Eight taps share a serial stereo delay line with a full feedback routing matrix — the architecture that creates complex, evolving delay textures you can't get from standard multi-tap delays.

**VST3 / AU** &middot; macOS &middot; Built with [JUCE 8](https://juce.com/)

## Features

**8-tap serial delay line** — Taps read from a shared delay line (not independent buffers), faithfully recreating the Verbos/Buchla signal flow. Drag tap positions freely or snap to a 10ms quantization grid. Delay range from ~10ms to ~5 seconds via base delay + multiplier.

**Feedback routing matrix** — Route any of the 8 individual taps or 4 preset mixes (Odd, Even, Rising, Falling) back into the delay input with independent gain per source. HP/LP filters shape the feedback tone. Two-stage limiting (tanh soft-clip + RMS energy limiter) prevents runaway oscillation while keeping things musical.

**Character control** — Dial in high-frequency roll-off and a subtle noise floor to move from pristine digital toward warm, BBD-like textures.

**Tempo sync** — Lock to host BPM with six note divisions (1/4, 1/8, dotted 1/8, triplet 1/8, 1/16, 1/2).

**38 automatable parameters** — Every control is exposed to the DAW. Per-sample parameter smoothing eliminates zipper noise.

**Tap presets** — Save and recall named tap position configurations stored in the plugin state.

## Building

Requires CMake 3.22+, a C++17 compiler, and the JUCE submodule.

```bash
git clone --recurse-submodules https://github.com/your-username/multi-tap-delay.git
cd multi-tap-delay

make            # Debug build, auto-installs to ~/Library/Audio/Plug-Ins/
make release    # Release build with LTO
make test       # Run test suite (Catch2)
make validate   # AU validation via auval
make clean      # Remove build directory
make uninstall  # Remove installed plugins
```

Plugins install to:
- `~/Library/Audio/Plug-Ins/VST3/Zeitraum.vst3`
- `~/Library/Audio/Plug-Ins/Components/Zeitraum.component`

## Architecture

```
Input ──→ [+ Feedback] ──→ Delay Line ──→ Tap 1 ──→ Tap 2 ──→ ... ──→ Tap 8
                ↑                              │        │                 │
                │                              ▼        ▼                 ▼
                │                          ┌─────────────────────────────────┐
                └────── Saturator ◄─────── │      Feedback Routing Matrix    │
                            │              │  8 taps + 4 preset mixes → gain │
                        HP/LP Filters      └─────────────────────────────────┘
```

Dual mono processing — each channel has its own delay line and filter state. DSP classes are header-only and JUCE-free where possible for easy unit testing.

### Source layout

```
src/
  PluginProcessor.h/cpp       # AudioProcessor, APVTS, state persistence
  PluginEditor.h/cpp          # Custom editor, layout, preset management
  dsp/
    DelayEngine.h             # Main DSP orchestrator
    FeedbackMatrix.h          # 12-source routing with per-source smoothing
    FeedbackSaturator.h       # tanh soft-clip + RMS energy limiter
    FeedbackFilter.h          # Dual-mono HP/LP pair
    TapReader.h               # Per-tap delay calculation and smoothing
    CharacterProcessor.h      # HF roll-off + noise floor
    OnePoleSmooth.h           # Exponential one-pole smoother
  ui/
    ZeitraumLookAndFeel.h     # Dark/teal theme
    TopBar.h                  # Global controls
    TapColumn.h               # Per-tap position bar + level fader
    TapPositionBar.h          # Draggable tap position display
    TapLevelFader.h           # Level fader component
    FeedbackMatrixEditor.h    # Feedback section layout
    FeedbackGainCell.h        # Individual feedback gain control
test/
  PluginTests.cpp             # Processor integration tests
  dsp/                        # DSP unit tests (one per class)
```

## Parameters

| Group | Parameters |
|-------|-----------|
| **Global** | Base Delay (10–150ms), Multiplier (1–33x), Mix, Character, Quantize, Tempo Sync, Note Division |
| **Taps 1–8** | Position (0–1), Level (0–1) per tap |
| **Feedback** | Gain per source (8 taps + Odd/Even/Rising/Falling), HP/LP frequency + on/off |
| **Output** | Mix preset selector (Manual / Odd / Even / Rising / Falling) |

## Credits

**Zeitraum** by Die stille Erde

Built with [JUCE](https://juce.com/) &middot; Tests with [Catch2](https://github.com/catchorg/Catch2)
