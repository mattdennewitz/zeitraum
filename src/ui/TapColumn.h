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

    // Editor-driven layout: place each sub-component in its designated zone
    void setPositionBarBounds(juce::Rectangle<int> bounds) { positionBar.setBounds(bounds); }
    void setLevelFaderBounds(juce::Rectangle<int> bounds) { levelFader.setBounds(bounds); }
    void setNumberLabelBounds(juce::Rectangle<int> bounds) { numberLabel.setBounds(bounds); }

    void resized() override
    {
        // Only used if setBounds is called on the TapColumn itself (fallback)
        auto bounds = getLocalBounds();

        auto labelArea = bounds.removeFromBottom(20);
        numberLabel.setBounds(labelArea);
        bounds.removeFromBottom(2);

        int faderHeight = juce::roundToInt(bounds.getHeight() * 0.25f);
        auto faderArea = bounds.removeFromBottom(faderHeight);
        levelFader.setBounds(faderArea);
        bounds.removeFromBottom(3);

        positionBar.setBounds(bounds);
    }

private:
    TapPositionBar positionBar;
    TapLevelFader levelFader;
    juce::Label numberLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapColumn)
};
