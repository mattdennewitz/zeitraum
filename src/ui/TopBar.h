#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TopBar : public juce::Component
{
public:
    explicit TopBar(juce::AudioProcessorValueTreeState& apvts)
    {
        // --- Sliders ---
        auto setupSlider = [this](juce::Slider& s, juce::Label& l,
                                   const juce::String& labelText,
                                   const juce::String& suffix)
        {
            s.setSliderStyle(juce::Slider::LinearBar);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
            s.setTextValueSuffix(" " + suffix);
            addAndMakeVisible(s);

            l.setText(labelText, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredRight);
            l.setFont(juce::FontOptions(12.0f));
            l.attachToComponent(&s, true);
            addAndMakeVisible(l);
        };

        setupSlider(baseDelaySlider, baseDelayLabel, "Delay", "ms");
        setupSlider(multiplierSlider, multiplierLabel, "Mult", "x");
        setupSlider(mixSlider, mixLabel, "Mix", "%");
        setupSlider(characterSlider, characterLabel, "Char", "%");

        // --- Toggle Buttons ---
        quantizeToggle.setButtonText("Quantize");
        addAndMakeVisible(quantizeToggle);

        tempoSyncToggle.setButtonText("Sync");
        addAndMakeVisible(tempoSyncToggle);

        // --- ComboBoxes (populate items BEFORE creating attachments) ---
        noteDivCombo.addItemList({"1/4", "1/8", "1/8 dot", "1/8 trip", "1/16", "1/2"}, 1);
        addAndMakeVisible(noteDivCombo);

        outputMixCombo.addItemList({"Manual", "Odd", "Even", "Rising", "Falling"}, 1);
        addAndMakeVisible(outputMixCombo);

        // --- Create attachments (AFTER all items are populated) ---
        baseDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "BASE_DELAY", baseDelaySlider);
        multiplierAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "MULTIPLIER", multiplierSlider);
        mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "MIX", mixSlider);
        characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "CHARACTER", characterSlider);

        quantizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "QUANTIZE", quantizeToggle);
        tempoSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "TEMPO_SYNC", tempoSyncToggle);

        noteDivAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "NOTE_DIV", noteDivCombo);
        outputMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "OUTPUT_MIX", outputMixCombo);
    }

    void resized() override
    {
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.flexWrap = juce::FlexBox::Wrap::noWrap;

        const float itemHeight = 24.0f;
        const float margin = 3.0f;

        // Sliders with labels -- labels are attached via attachToComponent, so give extra left margin
        fb.items.add(juce::FlexItem(baseDelaySlider).withFlex(1.5f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, 40.0f)));
        fb.items.add(juce::FlexItem(multiplierSlider).withFlex(1.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, 35.0f)));
        fb.items.add(juce::FlexItem(mixSlider).withFlex(1.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, 30.0f)));
        fb.items.add(juce::FlexItem(characterSlider).withFlex(1.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, 35.0f)));

        // Toggles
        fb.items.add(juce::FlexItem(quantizeToggle).withWidth(80.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, margin)));
        fb.items.add(juce::FlexItem(tempoSyncToggle).withWidth(60.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, margin)));

        // Combos
        fb.items.add(juce::FlexItem(noteDivCombo).withWidth(80.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, margin)));
        fb.items.add(juce::FlexItem(outputMixCombo).withWidth(90.0f).withHeight(itemHeight).withMargin(juce::FlexItem::Margin(0, margin, 0, margin)));

        fb.performLayout(getLocalBounds().reduced(6, 4));
    }

private:
    // Sliders
    juce::Slider baseDelaySlider;
    juce::Slider multiplierSlider;
    juce::Slider mixSlider;
    juce::Slider characterSlider;

    // Labels
    juce::Label baseDelayLabel;
    juce::Label multiplierLabel;
    juce::Label mixLabel;
    juce::Label characterLabel;

    // Toggle buttons
    juce::ToggleButton quantizeToggle;
    juce::ToggleButton tempoSyncToggle;

    // ComboBoxes
    juce::ComboBox noteDivCombo;
    juce::ComboBox outputMixCombo;

    // Attachments (must be destroyed before the components they reference)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> baseDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiplierAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> characterAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> quantizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tempoSyncAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noteDivAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outputMixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};
