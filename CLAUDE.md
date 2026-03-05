# Zeitraum - JUCE Plugin Development Guide

Multi-tap delay plugin with feedback routing matrix, built with JUCE 8.

## Project Identity

- **Plugin name:** Zeitraum
- **Manufacturer:** Die stille Erde
- **Manufacturer code:** `DsEr`
- **Plugin code:** `ZtRm`
- **Bundle ID:** `com.diestilleerde.zeitraum`
- **VST3 category:** Fx Delay
- **AU type:** `kAudioUnitType_Effect`
- **Formats:** VST3, AU

## Build Commands

```bash
make                    # Debug build + install to ~/Library/Audio/Plug-Ins/
make release            # Release build with LTO
make test               # Run Catch2 tests via ctest
make validate           # AU validation via auval
make clean              # Remove build directory
make uninstall          # Remove installed plugins
```

Plugin binaries auto-install to:
- `~/Library/Audio/Plug-Ins/VST3/Zeitraum.vst3`
- `~/Library/Audio/Plug-Ins/Components/Zeitraum.component`

## Code Conventions

### Namespacing

`DONT_SET_USING_JUCE_NAMESPACE=1` is set globally. Always use `juce::` prefix for all JUCE types.

### Source Layout

```
src/
  PluginProcessor.h/cpp    # AudioProcessor + APVTS
  PluginEditor.h/cpp        # UI (GenericAudioProcessorEditor in Phase 1)
  dsp/                      # Header-only DSP classes
  ui/                       # LookAndFeel, custom components
test/
  PluginTests.cpp           # Processor integration tests
  dsp/                      # DSP unit tests
```

- Header-only DSP classes in `src/dsp/` -- keep JUCE-free when possible for easier testing
- LookAndFeel and custom components in `src/ui/`
- PluginProcessor.h/cpp and PluginEditor.h/cpp at `src/` root

### Parameter Access

- Cache parameter pointers via `getRawParameterValue` in the constructor
- Read with `.load()` in processBlock (lock-free, realtime-safe)
- Never call `apvts.getParameter()->getValue()` on the audio thread
- Use `ParameterID{"NAME", 1}` with version numbers for forward compatibility

```cpp
// Constructor:
freqParam = apvts.getRawParameterValue("FREQ");

// processBlock:
float freq = freqParam->load();
```

## Architecture Notes

### Delay Architecture (Phases 2-4)

- Shared serial delay line (not independent buffers per tap)
- 8 taps along a stereo delay line (one line per channel)
- Feedback routing matrix: any tap or preset mix routed back to input
- Dual mono processing (separate delay line per channel)

### Parameter System

- APVTS for all parameters with ParameterID version numbers
- State persistence: XML with `pluginVersion` attribute for forward compatibility
- Guard all state restoration against null, corrupt, and wrong-tag XML

### Plugin Entry Point

PluginProcessor.cpp must end with:
```cpp
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZeitraumProcessor();
}
```

## Audio Thread Rules

### Never in processBlock

- Allocate memory (`new`, `std::vector::push_back`, `juce::String` construction)
- Lock mutexes
- Call `getParameter()` (use cached `getRawParameterValue` pointers)
- Log or print
- Touch UI components

### Always in processBlock

- `juce::ScopedNoDenormals` at the top
- Clear unused output channels at the bottom
- Use atomic loads for parameters (`.load()`)
- Smooth parameter changes (zipper noise is audible at ~100Hz rate)

### Parameter Smoothing

One-pole exponential smoother:
```
alpha = 1 - exp(-2*pi / (timeMs * 0.001 * sampleRate))
current += alpha * (target - current)
```

7-15ms typical for filter/delay parameters. 5ms for gain crossfades. Must be computed per-sample.

## Testing

- **Framework:** Catch2 v3.7.1 via FetchContent
- **Run:** `make test`
- **Test target** shares plugin compile definitions via `$<TARGET_PROPERTY:Zeitraum,COMPILE_DEFINITIONS>`
- Keep DSP classes JUCE-free when possible for easier unit testing
- Test files: `test/PluginTests.cpp` for integration, `test/dsp/` for DSP unit tests

### Test Patterns

```cpp
// Processor lifecycle
ZeitraumProcessor proc;
proc.prepareToPlay(44100.0, 512);
juce::AudioBuffer<float> buffer(2, 512);
juce::MidiBuffer midi;
proc.processBlock(buffer, midi);

// State round-trip
juce::MemoryBlock state;
proc.getStateInformation(state);
ZeitraumProcessor proc2;
proc2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
```

## Build System

- JUCE 8.0.12 as git submodule at `lib/JUCE`
- CMake 3.22+ with Ninja backend, wrapped by Makefile
- `COPY_PLUGIN_AFTER_BUILD=TRUE` for auto-install
- Linked modules: juce_audio_processors, juce_audio_utils, juce_gui_basics, juce_dsp

### AU Validation

After build changes, kill the AU cache before validation:
```bash
killall -9 AudioComponentRegistrar 2>/dev/null; true
sleep 1
auval -v aufx ZtRm DsEr
```
