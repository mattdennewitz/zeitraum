# Domain Pitfalls

**Domain:** JUCE multi-tap delay audio plugin with feedback matrix
**Researched:** 2026-03-05
**Confidence:** HIGH (based on established JUCE/DSP domain knowledge and reference project patterns)

## Critical Pitfalls

Mistakes that cause rewrites, AU validation failures, or shipping-blocking audio bugs.

### Pitfall 1: Feedback Matrix Instability (Runaway Oscillation)

**What goes wrong:** An NxN feedback routing matrix where any tap can feed back to the input with independent gain makes it trivially easy to create gain loops > 1.0. The delay line output explodes to infinity, producing ear-splitting noise or `NaN`/`inf` values that corrupt the entire signal chain. Users WILL set feedback gains that sum to >1.0 across multiple routing paths.

**Why it happens:** With 8 taps and cross-channel routing, the effective feedback gain is not the value on any single knob -- it is the spectral radius of the feedback matrix. Two taps each at 0.6 feeding back can sum to >1.0. Cross-channel routing creates even less obvious gain loops.

**Consequences:** Output clipping, `NaN` propagation (which infects every downstream sample), potential DAW crash, and if `NaN` reaches the AU output, `auval` will fail validation.

**Prevention:**
- Implement a hard output limiter at the delay line output (before feedback tap-off) as a safety net -- not for sound shaping, but to prevent runaway. A simple `juce::FloatVectorOperations::clip` or `std::clamp` on every sample in the feedback path.
- Add `NaN`/`inf` detection in `processBlock`: if detected, zero the delay buffer and reset. Use `std::isfinite()` checks on the feedback sum.
- Consider a soft-knee saturation (e.g., `tanh`) in the feedback path rather than hard clipping -- this is musically useful AND prevents blowup.
- Do NOT try to mathematically constrain the UI to prevent gain >1.0 -- the interaction effects are too complex and it limits creative use. Instead, make the system robust to hot feedback.

**Detection:** Test with all feedback gains at maximum. If you hear runaway oscillation within 2 seconds, the safety net is missing. Check for `NaN` in unit tests by running feedback at 0.99 gain for 10 seconds of simulated audio.

**Phase relevance:** Must be addressed in the core DSP engine phase, before any UI work. This is foundational.

---

### Pitfall 2: Delay Time Modulation Causes Clicks and Zipper Noise

**What goes wrong:** Changing delay time means changing the read position in the delay buffer. If you jump the read pointer discretely (even by 1 sample), you get a discontinuity in the output waveform -- an audible click. If you step through values without interpolation, you get zipper noise. The project spec explicitly requires "smooth delay time modulation with doppler/tape artifacts."

**Why it happens:** A delay line read position is an index into a circular buffer. Moving from reading at index 1000 to index 1050 means skipping 50 samples of audio history. That skip is a waveform discontinuity. Linear interpolation between old and new positions still produces artifacts if the position change per sample is too large.

**Consequences:** Audible clicks on any parameter change, unusable automation, fails the core "tape delay" character requirement.

**Prevention:**
- Use the ParameterSmoother pattern from the three-sisters project (one-pole exponential smoother) on the delay time parameter, but with a LONGER time constant than typical parameter smoothing (50-100ms, not 5-10ms). Delay time changes need to be slow enough to sound like tape speed changes.
- The read position must be a floating-point value, advanced smoothly sample-by-sample. Each sample, the read position moves by `1.0 + delta` where `delta` is the rate of delay time change. This naturally produces pitch-shifting doppler artifacts (the desired "tape" character).
- Use cubic Hermite or Lagrange interpolation for reading fractional positions from the delay buffer -- linear interpolation introduces audible high-frequency attenuation and noise at long delay times. `juce::dsp::DelayLine` supports `Lagrange3rd` interpolation type.
- NEVER snap the read pointer. Even when loading a preset with a different delay time, smoothly glide to the new position.

**Detection:** Automate the delay time parameter in a DAW with a sine wave input. Any clicking or stepping artifacts are immediately audible. Write a unit test that modulates delay time and checks for discontinuities (sample-to-sample differences exceeding a threshold).

**Phase relevance:** Core DSP engine phase. The interpolation strategy must be decided before implementing the delay line.

---

### Pitfall 3: AU Validation Failure from Tail Time and State Management

**What goes wrong:** Apple's `auval` tool is notoriously strict. Common failures for delay plugins: incorrect tail time reporting, failure to clear delay buffers on reset, outputting audio after the input stops (without declaring tail time), or producing `NaN`/`inf` values. A single `auval` failure means the plugin cannot ship as AU.

**Why it happens:** `getTailLengthSeconds()` must return the maximum possible delay time (not the current delay time). The three-sisters project returns `0.0` because filters have negligible tail -- a delay plugin cannot do this. Also, `prepareToPlay` must fully reinitialize all state, and `reset()` must zero all delay buffers.

**Consequences:** Plugin rejected by AU hosts (Logic Pro, GarageBand). Cannot ship on macOS without AU format.

**Prevention:**
- `getTailLengthSeconds()` must return the maximum delay time (with multiplier at max, ~1.2 seconds) PLUS enough time for feedback to decay. For a feedback gain of 0.95, that is roughly `maxDelay * log(threshold) / log(feedbackGain)`. A safe conservative value: return 10.0 seconds (or even 30.0 for high-feedback scenarios).
- `prepareToPlay()`: allocate and zero ALL delay buffers, reset all smoothers, reset all internal state.
- `reset()`: zero all delay buffers and reset smoothers to current target values (do NOT re-read parameters, just clear audio state).
- Never output denormals -- use `juce::ScopedNoDenormals` at the top of `processBlock()`. Denormals cause CPU spikes AND can trigger `auval` failures.
- The reference project Makefile already has the `auval` validation target with the `killall AudioComponentRegistrar` trick -- reuse this pattern.
- Run `auval -v aufx <code> <mfr>` after every significant DSP change, not just before release.

**Detection:** Run `make validate` early and often. The first time should be as soon as the plugin produces any audio output. Common `auval` failure messages to watch for: "Render failed", "Produced output after reset", "Tail time incorrect".

**Phase relevance:** Must be considered from the very first processBlock implementation. The Makefile validation target should be set up in the project scaffolding phase.

---

### Pitfall 4: Real-Time Thread Violations in processBlock

**What goes wrong:** `processBlock()` runs on the audio thread. Any operation that blocks -- memory allocation (`new`, `std::vector::push_back`), mutex locks, file I/O, Objective-C messaging (on macOS), or even `std::string` operations -- causes audio dropouts (glitches, clicks, silence gaps). This is the single most common mistake in JUCE plugin development.

**Why it happens:** C++ makes it easy to accidentally allocate. Common culprits in a delay plugin:
- Resizing delay buffers when sample rate changes (must happen in `prepareToPlay`, never in `processBlock`)
- Using `std::vector` for the feedback matrix routing (reallocation on modification)
- Calling `juce::AudioProcessorValueTreeState::getParameter()` (returns a pointer, fine) vs `.toString()` (allocates a string, NOT fine)
- Lambda captures that copy `std::string`
- Logging/debugging statements left in the audio path

**Consequences:** Audible glitches, especially at small buffer sizes (64 samples = 1.3ms at 48kHz -- zero margin for blocking operations). Professional users testing at 64-sample buffers will immediately notice.

**Prevention:**
- Use `std::atomic<float>` or `getRawParameterValue()` (returns `std::atomic<float>*`) to read parameters in `processBlock` -- exactly as done in the three-sisters reference project.
- Pre-allocate ALL buffers in `prepareToPlay()`. The delay buffer, temporary mixing buffers, feedback sum buffers -- everything.
- The feedback routing matrix should be a fixed-size `std::array<std::array<float, N>, N>` (where N=8 or 10 including preset mixes), not a dynamically-sized container.
- Use `juce::ScopedNoDenormals` at the top of every `processBlock`.
- On macOS: set the audio thread to real-time priority (JUCE does this for you, but do not fight it with blocking calls).
- Consider using a real-time-safe FIFO (e.g., `juce::AbstractFifo`) if the UI needs to send configuration changes to the audio thread (e.g., preset loading).

**Detection:** Use the Thread Sanitizer (`-fsanitize=thread`) during development. On macOS, Instruments "System Trace" can identify audio thread priority inversions. Test at 64-sample buffer size -- if it glitches there but not at 512, you have a real-time violation.

**Phase relevance:** Every phase that touches `processBlock`. Establish the pattern in the core DSP phase and never deviate.

---

### Pitfall 5: CLAP Support Requires clap-juce-extensions (Not Built Into JUCE)

**What goes wrong:** JUCE does not natively support CLAP format. Developers assume adding "CLAP" to the `FORMATS` list in `juce_add_plugin()` will work. It will not compile. CLAP requires a separate open-source wrapper library (`clap-juce-extensions`) integrated into the CMake build.

**Why it happens:** CLAP is a newer plugin format not yet adopted into the JUCE core. The three-sisters reference project does not include CLAP (it lists `VST3 AU Standalone`). Adding CLAP is additional build system work.

**Consequences:** Build failure if attempted naively. If deferred too long, integrating CLAP late can reveal incompatibilities with parameter handling or state save/restore.

**Prevention:**
- Add `clap-juce-extensions` as a git submodule (like JUCE itself) or use `FetchContent` in CMake.
- Integration pattern:
  ```cmake
  add_subdirectory(lib/clap-juce-extensions EXCLUDE_FROM_ALL)
  clap_juce_extensions_plugin(TARGET MultiTapDelay
      CLAP_ID "com.diestilleerde.multi-tap-delay"
      CLAP_FEATURES audio-effect delay)
  ```
- The CLAP ID must be a reverse-DNS identifier, unique to your plugin. Decide this early.
- CLAP exposes parameter metadata differently -- ensure all parameters have proper IDs, names, and ranges. The `ParameterID{"NAME", version}` pattern from three-sisters works well for this.
- Test CLAP in Bitwig (the DAW with the best CLAP support) and Reaper early.

**Detection:** Attempt a CLAP build as soon as the basic plugin skeleton compiles for VST3/AU. Do not wait until the end.

**Phase relevance:** Project scaffolding / build system phase. Get the CMake integration working with a pass-through plugin before building DSP.

---

## Moderate Pitfalls

### Pitfall 6: Shared Delay Line Architecture -- Write vs Read Ordering

**What goes wrong:** With 8 taps reading from a single shared delay line per channel, the order of operations matters. If you write the new input sample to the delay buffer before reading tap outputs, the shortest tap reads the just-written sample (effectively zero delay). If taps feed back into the input before the write, you get one-buffer-length of latency in the feedback path.

**Why it happens:** A circular buffer has one write head and multiple read heads. The write-then-read vs read-then-write ordering creates a one-sample difference that compounds in feedback paths.

**Prevention:**
- Use read-then-write ordering: read all tap outputs first, compute the feedback sum, mix with incoming audio, THEN write to the buffer. This ensures even the shortest tap has at least 1 sample of delay.
- Process sample-by-sample (not block-by-block) for the inner loop. The feedback computation requires the previous sample's tap outputs, so you cannot vectorize the feedback path across the entire block.
- Document the signal flow clearly in code comments.

**Detection:** Set a single tap at minimum delay with feedback at 0.9. If you hear a comb filter at `sampleRate/1` instead of `sampleRate/minDelay`, the ordering is wrong.

**Phase relevance:** Core DSP engine design. Must be correct from the start.

---

### Pitfall 7: Per-Tap Level Controls and Preset Mixes -- Gain Staging

**What goes wrong:** 8 individual tap levels, 4 preset mixes (odd, even, rising, falling), and a feedback matrix all multiply gains. Without proper gain staging, the output is either inaudibly quiet or massively clipping. Users combining multiple preset mixes with high feedback will easily hit +20dB or more.

**Why it happens:** Each gain stage is reasonable in isolation. 8 taps at unity gain already means +9dB if they are coherent. Add preset mix gains and feedback, and the headroom math gets complex.

**Prevention:**
- Normalize preset mixes: the "all odd taps" mix should divide by the number of active taps, not sum at unity.
- Add a master output level control with a sensible default (e.g., -6dB) to give headroom.
- Use per-tap gain ranges of 0.0 to 1.0 (not 0.0 to 2.0 or higher).
- Put a soft clipper or limiter on the final output as a safety net.

**Detection:** Feed a 0dBFS sine wave, enable all taps at unity, all preset mixes, and measure the output level. If it exceeds +6dB, gain staging needs work.

**Phase relevance:** DSP engine phase, but the UI/parameter design must account for it too.

---

### Pitfall 8: Delay Buffer Size Calculation Errors

**What goes wrong:** The delay buffer must be large enough to hold the maximum delay time at the maximum sample rate. Common mistake: calculating buffer size at 44.1kHz but the DAW runs at 96kHz or 192kHz, causing buffer overrun and reading garbage memory (or crashing).

**Why it happens:** `prepareToPlay` provides the current sample rate. If you hardcode a buffer size or forget to recalculate when sample rate changes, the buffer is too small at higher rates.

**Prevention:**
- Calculate buffer size as: `maxDelaySeconds * sampleRate + interpolationPadding`. For 1.2s max delay at 192kHz, that is 230,400 samples + padding for cubic interpolation (3-4 extra samples).
- Always recalculate in `prepareToPlay`. Round UP to the next power of 2 for efficient modulo-free wrapping (use bitwise AND instead of modulo for circular buffer indexing).
- JUCE's `juce::dsp::DelayLine` handles this if you use `setMaximumDelayInSamples()` in `prepare()`, but if rolling your own, do the math carefully.

**Detection:** Test at 192kHz sample rate. If you hear garbage audio or the plugin crashes, the buffer is undersized.

**Phase relevance:** Core DSP engine phase.

---

### Pitfall 9: State Save/Restore Losing Feedback Matrix Configuration

**What goes wrong:** `getStateInformation()` and `setStateInformation()` must serialize and restore the entire feedback routing matrix. If the matrix is not part of the `AudioProcessorValueTreeState` (APVTS) parameter tree, it will not be saved automatically. Users lose their carefully crafted feedback routings when reopening a project.

**Why it happens:** An 8x8 feedback matrix is 64 parameters (or more with cross-channel routing). Some developers store the matrix outside APVTS for convenience, then forget to serialize it. Others add the parameters to APVTS but use inconsistent parameter IDs between versions, breaking old sessions.

**Prevention:**
- Make every matrix gain a proper APVTS parameter with a stable `ParameterID`. Use a systematic naming scheme: `"FB_R0_C0"` through `"FB_R7_C7"` (or include channel: `"FB_L_R0_C0"`).
- The version number in `ParameterID{"FB_R0_C0", 1}` enables future parameter range changes without breaking saved sessions.
- Use APVTS's built-in XML serialization (`apvts.copyState().createXml()` and `apvts.replaceState()`) -- do not roll your own serialization.
- Test: save a preset, close the DAW, reopen, verify every matrix value restored correctly.

**Detection:** Automate this in a test: set non-default values for every parameter, serialize, deserialize, compare. Any mismatch is a bug.

**Phase relevance:** Parameter/state management phase. Design the parameter naming scheme before implementing the matrix.

---

### Pitfall 10: Cross-Channel Feedback Creates Mono Collapse

**What goes wrong:** When left-channel taps feed into the right channel and vice versa, repeated feedback iterations mix L and R together. After enough feedback loops, the stereo image collapses to mono. The delay output sounds "narrow" compared to the input.

**Why it happens:** Cross-feedback is literally mixing the channels. Each feedback iteration reduces the L/R difference. With high feedback gains, convergence to mono is fast.

**Prevention:**
- This is partly expected behavior and partly a design challenge. Mitigations:
  - Default cross-feedback gains to 0.0 (off), so users opt in.
  - Apply a subtle stereo width enhancement after the feedback path (e.g., a Haas effect or mid/side processing on the output).
  - Limit cross-feedback gain range to lower values than same-channel feedback (e.g., 0.0-0.5 vs 0.0-0.95).
  - Add a "stereo spread" control that applies slight delay time offsets between L and R taps.
- Document this behavior for users -- it is inherent to cross-feedback topology.

**Detection:** Feed a hard-panned signal (full left), enable cross-feedback at 0.7, listen to the output after 5-10 feedback iterations. If the signal is centered, mono collapse is happening.

**Phase relevance:** DSP engine phase, but the parameter range decisions affect UI design.

---

## Minor Pitfalls

### Pitfall 11: DONT_SET_USING_JUCE_NAMESPACE Inconsistency

**What goes wrong:** The three-sisters project sets `DONT_SET_USING_JUCE_NAMESPACE=1`, requiring all JUCE types to be prefixed with `juce::`. If this is set inconsistently between the plugin target and the test target, code compiles in one but not the other.

**Prevention:** Set this definition on both targets (plugin and test). Copy the pattern from three-sisters exactly: `target_compile_definitions` on both `MultiTapDelay` and `MultiTapDelayTests` with identical definitions.

**Phase relevance:** Project scaffolding phase.

---

### Pitfall 12: 10ms Tap Quantization Interacting Poorly with Sample Rates

**What goes wrong:** The spec calls for tap time quantization in 10ms steps. At 44.1kHz, 10ms = 441 samples. At 48kHz, 10ms = 480 samples. At 96kHz, 10ms = 960 samples. If quantization is implemented in samples rather than time, the behavior changes with sample rate.

**Prevention:** Always quantize in the time domain (milliseconds), then convert to samples. Store delay times as milliseconds internally, convert to sample counts in `prepareToPlay` and when the parameter changes.

**Detection:** Change the DAW sample rate and verify tap positions remain at the same time values.

**Phase relevance:** Core DSP engine phase.

---

### Pitfall 13: GUI Repainting Blocking the Message Thread

**What goes wrong:** The feedback matrix UI (potentially 64+ knobs or a grid control) triggers excessive repaints. If every parameter change repaints the entire matrix grid, the UI becomes sluggish, and because JUCE's message thread handles both UI and parameter dispatch, this can cause audio dropouts on some hosts.

**Prevention:**
- Use `repaint(Rectangle<int>)` to repaint only the changed cell, not the entire matrix.
- Throttle visual updates to ~30fps using a `juce::Timer`, reading parameter values in the timer callback and only repainting if values changed.
- Consider a custom component that draws the matrix as a single bitmap and only redraws dirty cells.
- Do NOT read audio-thread state directly from the paint method -- use `std::atomic` or a lock-free FIFO for meter/visualization data.

**Phase relevance:** UI implementation phase.

---

### Pitfall 14: Cubic Interpolation Reads Beyond Buffer Bounds

**What goes wrong:** Cubic (Hermite/Lagrange) interpolation needs 4 sample points (2 before and 1 after the target position, or 1 before and 2 after depending on the formulation). At the boundaries of the circular buffer, naive indexing reads outside the allocated memory.

**Prevention:**
- When using a circular buffer, always apply the wrapping mask/modulo to EVERY index used in the interpolation formula, not just the primary read index.
- If using `juce::dsp::DelayLine`, this is handled internally. If rolling your own, allocate `bufferSize + 4` samples and copy the wrap-around region, or use masked indexing.
- Write a test that sets the delay time such that the read position is exactly at the buffer boundary.

**Phase relevance:** Core DSP engine phase, delay line implementation.

---

### Pitfall 15: Multiplier Dial Creates Sudden Large Delay Jumps

**What goes wrong:** The spec mentions a "multiplier dial extending to ~1.2s total" from a base range of ~10-150ms. If the multiplier is applied as a discrete multiplication (e.g., 1x, 2x, 4x, 8x), changing the multiplier causes all tap positions to jump suddenly, creating a massive click even with delay time smoothing.

**Prevention:**
- Apply parameter smoothing to the multiplier value itself, not just to the final delay time.
- Or better: compute the final delay time (base * multiplier) and smooth THAT. The smoother will handle the large jump, but the smoothing time should be proportional to the jump size (use adaptive smoothing or a longer time constant for the multiplier).
- Consider making the multiplier continuous rather than stepped, to avoid any discontinuities.

**Detection:** Automate the multiplier parameter with a step change. If you hear a click, the smoothing is insufficient.

**Phase relevance:** Core DSP engine phase, parameter design.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Project scaffolding / build system | CLAP integration fails (Pitfall 5) | Add clap-juce-extensions submodule from day 1, verify all 3 formats build before writing DSP |
| Project scaffolding / build system | Namespace inconsistency (Pitfall 11) | Copy three-sisters CMake definitions exactly for both plugin and test targets |
| Core DSP engine | Feedback blowup (Pitfall 1) | Implement saturation/limiter in feedback path before any other DSP work |
| Core DSP engine | Delay time clicks (Pitfall 2) | Use fractional delay with cubic interpolation and one-pole smoothing from the start |
| Core DSP engine | Write/read ordering (Pitfall 6) | Read-then-write, sample-by-sample inner loop, document signal flow |
| Core DSP engine | Buffer size errors (Pitfall 8) | Calculate from max delay * max sample rate, test at 192kHz |
| Core DSP engine | Interpolation boundary errors (Pitfall 14) | Wrap all indices in interpolation formula, unit test at buffer boundaries |
| Core DSP engine | Multiplier clicks (Pitfall 15) | Smooth final delay time, not just base time |
| Parameter design | State save/restore (Pitfall 9) | Systematic parameter IDs for all 64+ matrix gains, all in APVTS |
| Parameter design | Gain staging (Pitfall 7) | Normalize preset mixes, default master level at -6dB |
| Parameter design | Quantization vs sample rate (Pitfall 12) | Quantize in ms, convert to samples per-rate |
| AU validation | Tail time, reset, denormals (Pitfall 3) | Set up `make validate` in scaffolding, run after every DSP change |
| Audio thread safety | Real-time violations (Pitfall 4) | Pre-allocate everything, use atomics, test at 64-sample buffers |
| Stereo processing | Mono collapse (Pitfall 10) | Default cross-feedback to 0, limit range, add stereo spread option |
| UI implementation | Repaint performance (Pitfall 13) | Timer-based throttled updates, dirty-cell repainting only |

## Sources

- JUCE `AudioProcessor` API documentation (getTailLengthSeconds, prepareToPlay, reset, processBlock contracts)
- JUCE `dsp::DelayLine` documentation (interpolation types, prepare/setMaximumDelayInSamples)
- Three-sisters reference project (`/Users/matt/src/three-sisters/`) -- CMakeLists.txt, PluginProcessor.cpp, ParameterSmoother.h patterns
- `clap-juce-extensions` GitHub repository (CMake integration pattern)
- Apple `auval` documentation (AU validation requirements)
- General DSP knowledge: circular buffer interpolation, feedback stability, gain staging principles
