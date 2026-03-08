#pragma once
#include "FeedbackGainCell.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class FeedbackMatrixEditor : public juce::Component
{
public:
    explicit FeedbackMatrixEditor(juce::AudioProcessorValueTreeState& apvts)
    {
        // Section header: "Taps"
        tapsHeader.setText("Taps", juce::dontSendNotification);
        tapsHeader.setFont(juce::FontOptions(12.0f));
        tapsHeader.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        tapsHeader.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(tapsHeader);

        // 8 tap gain cells
        for (int i = 0; i < 8; ++i)
        {
            juce::String paramID = "FB_TAP" + juce::String(i + 1);
            juce::String label = "Tap " + juce::String(i + 1);
            tapCells[static_cast<size_t>(i)] = std::make_unique<FeedbackGainCell>(
                *apvts.getParameter(paramID), label);
            addAndMakeVisible(*tapCells[static_cast<size_t>(i)]);
        }

        // Section header: "Mixes"
        mixesHeader.setText("Mixes", juce::dontSendNotification);
        mixesHeader.setFont(juce::FontOptions(12.0f));
        mixesHeader.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        mixesHeader.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(mixesHeader);

        // 4 preset mix gain cells
        static const char* mixParamIDs[] = { "FB_ODD", "FB_EVEN", "FB_RISING", "FB_FALLING" };
        static const char* mixLabels[] = { "Odd", "Even", "Rising", "Falling" };

        for (int i = 0; i < 4; ++i)
        {
            mixCells[static_cast<size_t>(i)] = std::make_unique<FeedbackGainCell>(
                *apvts.getParameter(mixParamIDs[i]), mixLabels[i]);
            addAndMakeVisible(*mixCells[static_cast<size_t>(i)]);
        }

        // --- Filter controls ---
        auto setupFilterSlider = [this](juce::Slider& s, const juce::String& suffix)
        {
            s.setSliderStyle(juce::Slider::LinearBar);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
            s.setTextValueSuffix(" " + suffix);
            addAndMakeVisible(s);
        };

        setupFilterSlider(hpFreqSlider, "Hz");
        setupFilterSlider(lpFreqSlider, "Hz");

        hpOnToggle.setButtonText("HP");
        addAndMakeVisible(hpOnToggle);

        lpOnToggle.setButtonText("LP");
        addAndMakeVisible(lpOnToggle);

        hpLabel.setText("HP", juce::dontSendNotification);
        hpLabel.setFont(juce::FontOptions(11.0f));
        hpLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        hpLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(hpLabel);

        lpLabel.setText("LP", juce::dontSendNotification);
        lpLabel.setFont(juce::FontOptions(11.0f));
        lpLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        lpLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(lpLabel);

        // Create attachments
        hpFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "FB_HP_FREQ", hpFreqSlider);
        lpFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "FB_LP_FREQ", lpFreqSlider);
        hpOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "FB_HP_ON", hpOnToggle);
        lpOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "FB_LP_ON", lpOnToggle);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const int cellHeight = 24;
        const int headerHeight = 18;
        const int cellGap = 2;
        const int sectionGap = 4;

        // "Taps" header
        tapsHeader.setBounds(bounds.removeFromTop(headerHeight));
        bounds.removeFromTop(2);

        // 8 tap cells
        for (int i = 0; i < 8; ++i)
        {
            tapCells[static_cast<size_t>(i)]->setBounds(bounds.removeFromTop(cellHeight));
            bounds.removeFromTop(cellGap);
        }

        // Section gap
        bounds.removeFromTop(sectionGap);

        // "Mixes" header
        mixesHeader.setBounds(bounds.removeFromTop(headerHeight));
        bounds.removeFromTop(2);

        // 4 mix cells
        for (int i = 0; i < 4; ++i)
        {
            mixCells[static_cast<size_t>(i)]->setBounds(bounds.removeFromTop(cellHeight));
            bounds.removeFromTop(cellGap);
        }

        // Filter controls in remaining space
        bounds.removeFromTop(sectionGap);

        const int filterRowHeight = 22;
        const int labelWidth = 24;
        const int toggleWidth = 44;
        const int filterGap = 4;

        // HP row
        auto hpRow = bounds.removeFromTop(filterRowHeight);
        hpLabel.setBounds(hpRow.removeFromLeft(labelWidth));
        hpRow.removeFromLeft(2);
        hpOnToggle.setBounds(hpRow.removeFromLeft(toggleWidth));
        hpRow.removeFromLeft(filterGap);
        hpFreqSlider.setBounds(hpRow);

        bounds.removeFromTop(4);

        // LP row
        auto lpRow = bounds.removeFromTop(filterRowHeight);
        lpLabel.setBounds(lpRow.removeFromLeft(labelWidth));
        lpRow.removeFromLeft(2);
        lpOnToggle.setBounds(lpRow.removeFromLeft(toggleWidth));
        lpRow.removeFromLeft(filterGap);
        lpFreqSlider.setBounds(lpRow);
    }

private:
    // Section headers
    juce::Label tapsHeader;
    juce::Label mixesHeader;

    // Gain cells
    std::array<std::unique_ptr<FeedbackGainCell>, 8> tapCells;
    std::array<std::unique_ptr<FeedbackGainCell>, 4> mixCells;

    // Filter controls
    juce::Slider hpFreqSlider;
    juce::Slider lpFreqSlider;
    juce::ToggleButton hpOnToggle;
    juce::ToggleButton lpOnToggle;
    juce::Label hpLabel;
    juce::Label lpLabel;

    // Filter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lpOnAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedbackMatrixEditor)
};
