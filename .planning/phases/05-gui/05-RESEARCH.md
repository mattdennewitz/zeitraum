# Phase 5: GUI - Research

**Researched:** 2026-03-08
**Domain:** JUCE custom GUI components, LookAndFeel, parameter attachments
**Confidence:** HIGH

## Summary

Phase 5 replaces the placeholder editor with a full custom GUI for the Zeitraum plugin. The existing codebase has all parameters wired through APVTS with cached atomic pointers, tap preset save/recall methods on the processor, and an empty `src/ui/` directory ready for custom components. The current editor is a bare 400x300 dark rectangle.

The GUI requires three custom component types not available in stock JUCE: (1) vertical bar-graph tap position editor with drag-to-reposition, (2) vertical level faders per tap, and (3) a feedback matrix gain editor with horizontal fill bars. All three need custom `ParameterAttachment` wiring to APVTS parameters. Standard JUCE `Slider` and `ComboBox` components with their built-in attachments handle the top-bar global controls. Layout uses `juce::FlexBox` for the top bar and main left/right split, with manual `setBounds` for the per-tap column grid where precise alignment between position bars and level faders matters.

**Primary recommendation:** Build three custom `juce::Component` subclasses (TapPositionBar, TapLevelFader, FeedbackMatrixEditor) each using `juce::ParameterAttachment` for APVTS binding, wrapped in a single custom `juce::LookAndFeel_V4` subclass for the dark/teal theme. Use stock `juce::Slider` (LinearBar style) and `juce::ToggleButton` with `SliderAttachment`/`ButtonAttachment` for global controls.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Vertical bar graph style -- 8 columns, one per tap
- Bar height represents tap position (delay time along the shared delay line)
- Drag the top edge of each bar to reposition taps
- Level faders directly below each position bar (stacked column per tap)
- Millisecond labels on each bar + grid lines when quantize is enabled
- Taps differentiated by number labels (1-8), all same accent color
- Window size: 900x500, resizable with minimum size (~700x400)
- Top bar: horizontal strip with linear sliders for global controls (base delay, multiplier, mix, character, quantize toggle, tempo sync toggle, note division, output mix preset selector)
- No plugin branding in the GUI -- DAW shows name in window title
- Main content: left side (~60%) = tap position bars + level faders, right side (~40%) = feedback matrix
- Feedback filter controls (HP/LP freq sliders, on/off toggles) below the feedback matrix in the right panel
- Feedback matrix: vertical list of gain cells -- one row per feedback source (8 taps, 4 preset mixes)
- Section labels: "Taps" header above tap rows, "Mixes" header above preset mix rows
- Each cell is a horizontal fill bar showing gain percentage
- Click and drag horizontally to set gain
- Double-click any cell to type a precise value
- Static color fill (no signal metering/glow)
- Dark theme (dark background, matching current #2D2D2D base)
- Teal/cyan accent color for active elements (bars, faders, gain fills)
- Inactive/zero-gain elements dimmed (lower opacity) but always visible
- Single accent color + numbered labels to differentiate taps (not color-coded per tap)
- Always-visible value labels on all controls (ms, %, dB as appropriate)
- Double-click-to-type available on all controls (sliders, bars, faders, matrix cells)
- Tap presets: named presets (user types a name when saving), dropdown + save button

### Claude's Discretion
- Exact spacing, margins, and typography
- Tap preset UI placement (top bar vs above tap bars)
- Loading/empty states
- Exact minimum window dimensions
- Font choices and sizes
- Scroll behavior if window is at minimum size

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| GUI-01 | Clean/modern DAW-style GUI (not skeuomorphic) | Custom LookAndFeel_V4 subclass with dark/teal ColourScheme; FlexBox layout; no textures or gradients |
| GUI-02 | Visual display of tap positions and timing | Custom TapPositionBar component with drag interaction, ms labels, quantize grid lines, ParameterAttachment to TAP*_POS params |
| GUI-03 | Per-tap level controls visible and adjustable in GUI | Custom TapLevelFader component below each position bar, ParameterAttachment to TAP*_LEVEL params |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::LookAndFeel_V4 | JUCE 8.0.12 | Theme and colour management | Built-in V4 supports ColourScheme with 9 UI colours; subclass for custom dark/teal theme |
| juce::ParameterAttachment | JUCE 8.0.12 | Custom component to APVTS binding | Official low-level attachment class; used by SliderParameterAttachment internally |
| juce::SliderParameterAttachment | JUCE 8.0.12 | Stock slider to APVTS binding | Built-in for standard sliders (top bar controls) |
| juce::ButtonParameterAttachment | JUCE 8.0.12 | Toggle button to APVTS binding | Built-in for quantize/tempo sync/HP on/LP on toggles |
| juce::ComboBoxParameterAttachment | JUCE 8.0.12 | Dropdown to APVTS binding | Built-in for note division and output mix selectors |
| juce::FlexBox | JUCE 8.0.12 | Responsive layout | CSS-like flex layout for top bar and main panel split |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce::Label | JUCE 8.0.12 | Text display + editable text input | Value labels, section headers, double-click-to-type overlay |
| juce::TextEditor | JUCE 8.0.12 | Text input for preset naming | Save-preset name entry |
| juce::ComboBox | JUCE 8.0.12 | Dropdown menus | Note division, output mix, tap preset selector |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| FlexBox for all layout | juce::Grid | Grid is better for 2D grids but FlexBox handles the top-bar-row + main-content-split more naturally; use manual setBounds for the 8-column tap grid where pixel-precise alignment is needed |
| Custom ParameterAttachment | addParameterListener | ParameterAttachment handles gesture begin/end automatically; addParameterListener requires manual gesture management |
| Custom bar components | juce::Slider (LinearBar) | Slider can be styled via LookAndFeel but lacks the "drag only the top edge" interaction needed for tap position bars |

## Architecture Patterns

### Recommended Project Structure
```
src/
  PluginProcessor.h/cpp        # Existing -- no changes needed
  PluginEditor.h/cpp           # Rebuilt -- owns all child components
  ui/
    ZeitraumLookAndFeel.h      # LookAndFeel_V4 subclass (dark/teal theme)
    TapPositionBar.h           # Custom component: vertical bar, drag top edge
    TapLevelFader.h            # Custom component: vertical fader below position bar
    TapColumn.h                # Composite: position bar + level fader + label
    FeedbackMatrixEditor.h     # Custom component: 12-row gain cell list
    FeedbackGainCell.h         # Single horizontal fill-bar gain cell
    TopBar.h                   # Composite: global controls strip
```

### Pattern 1: Custom ParameterAttachment for Drag Components
**What:** Each custom component (TapPositionBar, TapLevelFader, FeedbackGainCell) owns a `juce::ParameterAttachment` that syncs the component's visual state with the APVTS parameter.
**When to use:** Any custom component that controls an APVTS parameter but is not a stock Slider/Button/ComboBox.
**Example:**
```cpp
// Source: JUCE 8.0.12 juce_ParameterAttachments.h (verified in local submodule)
class TapPositionBar : public juce::Component
{
public:
    TapPositionBar(juce::RangedAudioParameter& param)
        : attachment(param,
                     [this](float newValue) { setValue(newValue); },
                     nullptr)
    {
        attachment.sendInitialUpdate();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        attachment.beginGesture();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        float newValue = /* map e.y to 0..1 range */;
        attachment.setValueAsPartOfGesture(newValue);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        attachment.endGesture();
    }

private:
    void setValue(float newValue)
    {
        currentValue = newValue;
        repaint();
    }

    float currentValue = 0.0f;
    juce::ParameterAttachment attachment;
};
```

### Pattern 2: Custom LookAndFeel_V4 with ColourScheme
**What:** Single LookAndFeel subclass owned by the editor, applied to all child components, defining the dark/teal colour palette and overriding draw methods for sliders and toggle buttons.
**When to use:** Plugin-wide theming.
**Example:**
```cpp
// Source: JUCE 8.0.12 juce_LookAndFeel_V4.h (verified in local submodule)
class ZeitraumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZeitraumLookAndFeel()
        : juce::LookAndFeel_V4(juce::LookAndFeel_V4::ColourScheme{
              0xff2d2d2d,  // windowBackground
              0xff3a3a3a,  // widgetBackground
              0xff252525,  // menuBackground
              0xff555555,  // outline
              0xffcccccc,  // defaultText
              0xff00bcd4,  // defaultFill (teal accent)
              0xffffffff,  // highlightedText
              0xff00acc1,  // highlightedFill
              0xffcccccc   // menuText
          })
    {
        // Additional per-component colour overrides as needed
    }

    // Override drawLinearSlider, drawToggleButton, etc. for custom rendering
};
```

### Pattern 3: Resizable Editor with Constraints
**What:** AudioProcessorEditor configured with setResizable + setResizeLimits for responsive layout.
**When to use:** The editor constructor.
**Example:**
```cpp
// Source: JUCE AudioProcessorEditor docs (verified via web search)
ZeitraumEditor::ZeitraumEditor(ZeitraumProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p)
{
    setResizable(true, true);
    setResizeLimits(700, 400, 1400, 900);
    setSize(900, 500);
    setLookAndFeel(&lookAndFeel);
}

ZeitraumEditor::~ZeitraumEditor()
{
    setLookAndFeel(nullptr);
}
```

### Pattern 4: Double-Click-to-Type on Custom Components
**What:** On double-click, show a juce::Label in editable mode overlaying the component. On commit, parse the text and update the parameter.
**When to use:** TapPositionBar, TapLevelFader, FeedbackGainCell -- all custom components that need precise value entry.
**Example:**
```cpp
void mouseDoubleClick(const juce::MouseEvent&) override
{
    auto* label = new juce::Label();
    label->setEditable(true, true, false);
    label->setText(juce::String(currentValue, 1), juce::dontSendNotification);
    label->setBounds(getLocalBounds().reduced(2));
    label->onTextChange = [this, label]()
    {
        float newVal = label->getText().getFloatValue();
        attachment.setValueAsCompleteGesture(newVal);
    };
    label->onEditorHide = [this, label]()
    {
        juce::MessageManager::callAsync([this, label]() {
            removeChildComponent(label);
            delete label;
        });
    };
    addAndMakeVisible(label);
    label->showEditor();
}
```

### Pattern 5: Tap Preset Management via Processor Methods
**What:** The editor calls processor.saveTapPreset(name), processor.recallTapPreset(name), and processor.getTapPresetNames() -- no new APVTS parameters needed. ComboBox populated from getTapPresetNames(), save button triggers TextEditor for name input.
**When to use:** Tap preset dropdown + save button.

### Anti-Patterns to Avoid
- **Calling getRawParameterValue in the editor:** Use ParameterAttachment or SliderAttachment instead. Raw pointers are for the audio thread only.
- **Allocating components dynamically on resize:** Create all child components in the constructor, only reposition in resized().
- **Setting LookAndFeel on individual components:** Set once on the editor; children inherit automatically via Component::getLookAndFeel() parent traversal.
- **Using juce::Timer for parameter polling:** ParameterAttachment pushes changes to the message thread automatically via AsyncUpdater.
- **Forgetting setLookAndFeel(nullptr) in destructor:** Components must clear their LookAndFeel reference before the LookAndFeel object is destroyed, or you get use-after-free crashes.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter-to-UI binding | Manual addParameterListener + gesture tracking | juce::ParameterAttachment | Handles denormalization, gesture begin/end, async message thread updates, avoids race conditions |
| Slider-to-parameter sync | Custom value change listeners | juce::SliderParameterAttachment (via APVTS::SliderAttachment typedef) | Handles all edge cases: drag gestures, host automation, undo manager |
| Colour theming | Per-component setColour calls | LookAndFeel_V4 ColourScheme + subclass | Single source of truth; all components inherit; easy to change |
| Layout math | Manual pixel arithmetic | juce::FlexBox for flex sections, proportional bounds for fixed grids | Handles resizing correctly; less error-prone |

**Key insight:** JUCE's attachment classes do more than simple value forwarding -- they handle the gesture protocol (beginChangeGesture/endChangeGesture) that DAW hosts require for proper automation recording. Rolling your own risks broken automation.

## Common Pitfalls

### Pitfall 1: LookAndFeel Lifetime
**What goes wrong:** Crash on plugin close -- use-after-free when child components try to access a destroyed LookAndFeel.
**Why it happens:** LookAndFeel_V4 subclass is a member of the editor, destroyed before child components if declared after them.
**How to avoid:** Declare LookAndFeel as the FIRST member of the editor class (constructed first, destroyed last), OR call `setLookAndFeel(nullptr)` in the editor destructor before children are destroyed.
**Warning signs:** Intermittent crash on plugin window close in debug builds.

### Pitfall 2: ComboBox Items Before Attachment
**What goes wrong:** ComboBox shows wrong initial selection or triggers unexpected parameter change on construction.
**Why it happens:** ComboBoxParameterAttachment sends an initial update; if the ComboBox has no items yet, it maps to index 0.
**How to avoid:** Always populate ComboBox items BEFORE creating the ComboBoxParameterAttachment.
**Warning signs:** Parameter jumps to first value when editor opens.

### Pitfall 3: Resizing Breaks Layout
**What goes wrong:** Components overlap or disappear at minimum window size.
**Why it happens:** Layout code uses absolute positions instead of proportional calculations.
**How to avoid:** Use proportional splits (e.g., 60%/40% for left/right panels) and FlexBox with flex-grow/flex-shrink. Test at minimum size (700x400) explicitly.
**Warning signs:** GUI looks correct only at the default 900x500 size.

### Pitfall 4: Double-Click-to-Type Focus Issues
**What goes wrong:** Label editor doesn't receive keyboard focus, or typing goes to the wrong component.
**Why it happens:** Plugin host may intercept keyboard events before they reach the editor.
**How to avoid:** Call `label->grabKeyboardFocus()` after `showEditor()`. Set `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` in CMake (already set) but ensure the Label's internal TextEditor gets focus.
**Warning signs:** Typing has no effect; keys trigger DAW shortcuts instead.

### Pitfall 5: Parameter Value Display Units
**What goes wrong:** Labels show raw 0-1 normalized values instead of human-readable ms/% values.
**Why it happens:** ParameterAttachment callback receives denormalized values, but tap position is 0-1 and needs conversion to ms using base delay and multiplier.
**How to avoid:** In the callback, compute display value: `ms = position * baseDelay * multiplier`. Read baseDelay and multiplier from the APVTS (on message thread, safe via getParameter()->getValue()).
**Warning signs:** Position bars show "0.125" instead of "10.0 ms".

### Pitfall 6: createEditor Returns GenericEditor
**What goes wrong:** Custom GUI never appears; plugin shows the generic parameter list.
**Why it happens:** Current `createEditor()` returns `new juce::GenericAudioProcessorEditor(*this)`.
**How to avoid:** Change `createEditor()` to return `new ZeitraumEditor(*this)`.
**Warning signs:** Plugin window shows a long scrollable list of sliders.

## Code Examples

### Complete Custom Attachment Pattern
```cpp
// Source: juce_ParameterAttachments.h (JUCE 8.0.12 local submodule)
// This shows how SliderParameterAttachment works internally,
// serving as the template for custom attachments:

class SliderParameterAttachment : private juce::Slider::Listener
{
public:
    SliderParameterAttachment(juce::RangedAudioParameter& parameter,
                               juce::Slider& s,
                               juce::UndoManager* undoManager = nullptr)
        : slider(s),
          attachment(parameter,
                     [this](float f) { setValue(f); },
                     undoManager)
    {
        slider.valueFromTextFunction = /* ... */;
        slider.textFromValueFunction = /* ... */;
        slider.setDoubleClickReturnValue(true, parameter.getDefaultValue());

        auto range = parameter.getNormalisableRange();
        // ... configure slider range ...

        slider.addListener(this);
        attachment.sendInitialUpdate();
    }

    ~SliderParameterAttachment() override { slider.removeListener(this); }

private:
    void setValue(float newValue)
    {
        juce::ScopedValueSetter<bool> svs(ignoreCallbacks, true);
        slider.setValue(newValue, juce::sendNotificationSync);
    }

    void sliderValueChanged(juce::Slider*) override
    {
        if (!ignoreCallbacks)
            attachment.setValueAsPartOfGesture((float)slider.getValue());
    }

    void sliderDragStarted(juce::Slider*) override { attachment.beginGesture(); }
    void sliderDragEnded(juce::Slider*) override   { attachment.endGesture(); }

    juce::Slider& slider;
    juce::ParameterAttachment attachment;
    bool ignoreCallbacks = false;
};
```

### FlexBox Top Bar Layout
```cpp
// Source: JUCE FlexBox tutorial pattern
void TopBar::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    fb.alignItems = juce::FlexBox::AlignItems::center;

    fb.items.add(juce::FlexItem(baseDelaySlider).withFlex(1.5f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(multiplierSlider).withFlex(1.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(mixSlider).withFlex(1.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(characterSlider).withFlex(1.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(quantizeToggle).withWidth(60.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(tempoSyncToggle).withWidth(70.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(noteDivCombo).withWidth(80.0f).withHeight(24.0f));
    fb.items.add(juce::FlexItem(outputMixCombo).withWidth(90.0f).withHeight(24.0f));

    fb.performLayout(getLocalBounds().reduced(4));
}
```

### Resizable Editor Main Layout
```cpp
void ZeitraumEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top bar: fixed height
    auto topBarArea = bounds.removeFromTop(50);
    topBar.setBounds(topBarArea);

    // Main content area with padding
    auto mainArea = bounds.reduced(8, 4);

    // Left 60%: tap columns
    auto leftArea = mainArea.removeFromLeft(
        juce::roundToInt(mainArea.getWidth() * 0.6f));
    tapColumnsPanel.setBounds(leftArea);

    // Right 40%: feedback matrix + filter controls
    auto rightArea = mainArea;
    auto filterArea = rightArea.removeFromBottom(80);
    feedbackMatrix.setBounds(rightArea);
    filterControls.setBounds(filterArea);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| addAndMakeVisible + manual setBounds everywhere | FlexBox/Grid for layout | JUCE 5+ | Cleaner resizing, less layout math |
| AudioProcessorParameter::Listener + manual begin/endGesture | ParameterAttachment class | JUCE 6 | Encapsulates gesture protocol, less boilerplate |
| LookAndFeel_V3 custom paint | LookAndFeel_V4 with ColourScheme | JUCE 5 | 9-colour palette, less override code needed |
| GenericAudioProcessorEditor for prototyping | Custom editor from start | N/A (project evolution) | Phase 1 used Generic; Phase 5 replaces with custom |

**Deprecated/outdated:**
- `AudioProcessorValueTreeState::createAndAddParameter()` -- replaced by `ParameterLayout` in constructor (already used correctly in this project)
- `LookAndFeel_V2` / `LookAndFeel_V3` -- V4 is current; earlier versions require more overrides

## Open Questions

1. **Tap position ms display calculation**
   - What we know: Position is 0-1 normalized; actual ms = position * baseDelay * multiplier
   - What's unclear: Whether to read baseDelay/multiplier from APVTS on the message thread for display, or add a listener to update display values reactively
   - Recommendation: Use APVTS parameter listeners for baseDelay and multiplier; when either changes, recalculate all tap ms labels. Message thread access to getParameter()->getValue() is safe.

2. **Tap preset dropdown: populate dynamically**
   - What we know: getTapPresetNames() returns current names; presets stored in ValueTree
   - What's unclear: How to detect when a new preset is saved and refresh the dropdown
   - Recommendation: After saveTapPreset(), immediately repopulate the ComboBox from getTapPresetNames(). Alternatively, listen to ValueTree changes on the TapPresets node.

3. **Grid lines for quantize mode**
   - What we know: When quantize is on, tap times snap to 10ms increments
   - What's unclear: Exact grid line spacing depends on current base delay and multiplier
   - Recommendation: Calculate grid lines dynamically: for each 10ms increment within the visible range (0 to baseDelay * multiplier), draw a horizontal line at the corresponding y-position. Show/hide based on quantize parameter state.

## Sources

### Primary (HIGH confidence)
- JUCE 8.0.12 local submodule (`lib/JUCE/modules/juce_audio_processors/utilities/juce_ParameterAttachments.h`) -- ParameterAttachment API, SliderParameterAttachment pattern
- JUCE 8.0.12 local submodule (`lib/JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V4.h`) -- ColourScheme struct, UIColour enum
- Existing codebase (`src/PluginProcessor.h/cpp`, `src/PluginEditor.h/cpp`) -- current parameter layout, editor shell, integration points

### Secondary (MEDIUM confidence)
- [JUCE AudioProcessorEditor docs](https://docs.juce.com/master/classAudioProcessorEditor.html) -- setResizable, setResizeLimits API
- [JUCE ParameterAttachment docs](https://docs.juce.com/master/classParameterAttachment.html) -- gesture protocol, custom attachment guidance
- [JUCE FlexBox/Grid tutorial](https://juce.com/tutorials/tutorial_flex_box_grid) -- layout patterns
- [JUCE LookAndFeel_V4 docs](https://docs.juce.com/master/classLookAndFeel__V4.html) -- ColourScheme static methods

### Tertiary (LOW confidence)
- [JUCE Forum: custom ParameterAttachment](https://forum.juce.com/t/custom-audioprocessorvaluetreestate-attachment/38194) -- community patterns for custom attachment classes
- [JUCE Forum: resizable editor issues](https://forum.juce.com/t/setresizable-true-false-working-for-vst3-but-not-vst2-au/38104) -- AU/VST3 resize behavior differences

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components from JUCE 8.0.12 local submodule, APIs verified in source
- Architecture: HIGH -- patterns verified from SliderParameterAttachment source code and official docs
- Pitfalls: HIGH -- LookAndFeel lifetime, ComboBox init order, and createEditor fix are well-documented JUCE patterns

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (JUCE 8.x stable, no breaking changes expected)
