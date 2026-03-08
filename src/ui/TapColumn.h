#pragma once
#include "TapPositionBar.h"
#include "TapLevelFader.h"

class TapColumn : public juce::Component
{
public:
    TapColumn(int tapIndex, juce::AudioProcessorValueTreeState& apvts)
        : positionBar(*apvts.getParameter("TAP" + juce::String(tapIndex + 1) + "_POS"), apvts),
          levelFader(*apvts.getParameter("TAP" + juce::String(tapIndex + 1) + "_LEVEL"))
    {
        addAndMakeVisible(positionBar);
        addAndMakeVisible(levelFader);

        numberLabel.setText(juce::String(tapIndex + 1), juce::dontSendNotification);
        numberLabel.setJustificationType(juce::Justification::centred);
        numberLabel.setFont(juce::FontOptions(13.0f));
        numberLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        addAndMakeVisible(numberLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Number label at bottom: 20px
        auto labelArea = bounds.removeFromBottom(20);
        numberLabel.setBounds(labelArea);

        // Small gap
        bounds.removeFromBottom(2);

        // Level fader: 25% of remaining height
        int faderHeight = juce::roundToInt(bounds.getHeight() * 0.25f);
        auto faderArea = bounds.removeFromBottom(faderHeight);
        levelFader.setBounds(faderArea);

        // Small gap
        bounds.removeFromBottom(3);

        // Position bar: remaining height
        positionBar.setBounds(bounds);
    }

private:
    TapPositionBar positionBar;
    TapLevelFader levelFader;
    juce::Label numberLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapColumn)
};
