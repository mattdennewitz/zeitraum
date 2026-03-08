#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class FeedbackGainCell : public juce::Component
{
public:
    FeedbackGainCell(juce::RangedAudioParameter& gainParam, const juce::String& label)
        : sourceLabel(label),
          attachment(gainParam,
                     [this](float newValue) { setValue(newValue); },
                     nullptr)
    {
        setOpaque(false);
        attachment.sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background track (dim)
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRoundedRectangle(bounds, 2.0f);

        // Teal fill from left
        float fillWidth = currentValue * bounds.getWidth();
        float minFillWidth = 2.0f;
        float displayFillWidth = juce::jmax(fillWidth, minFillWidth);

        auto fillRect = bounds.withWidth(displayFillWidth);

        // Dim if value is zero
        float alpha = (currentValue < 0.01f) ? 0.3f : 1.0f;
        g.setColour(juce::Colour(0xff00bcd4).withAlpha(alpha));
        g.fillRoundedRectangle(fillRect, 2.0f);

        // Source label on the left
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.9f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(sourceLabel, bounds.reduced(4.0f, 0.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        // Percentage text on the right
        int pct = static_cast<int>(currentValue * 100.0f + 0.5f);
        juce::String pctText = juce::String(pct) + "%";
        g.drawText(pctText, bounds.reduced(4.0f, 0.0f).toNearestInt(),
                   juce::Justification::centredRight, false);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        attachment.beginGesture();
        dragging = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;

        float newValue = e.position.x / static_cast<float>(getWidth());
        newValue = juce::jlimit(0.0f, 1.0f, newValue);

        ignoreCallbacks = true;
        attachment.setValueAsPartOfGesture(newValue);
        ignoreCallbacks = false;
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging)
        {
            attachment.endGesture();
            dragging = false;
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        int pct = static_cast<int>(currentValue * 100.0f + 0.5f);

        auto* label = new juce::Label();
        label->setEditable(true, true, false);
        label->setText(juce::String(pct), juce::dontSendNotification);
        label->setBounds(getLocalBounds().reduced(1));
        label->setFont(juce::FontOptions(11.0f));
        label->setColour(juce::Label::backgroundColourId, juce::Colour(0xff252525));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        label->setColour(juce::Label::outlineColourId, juce::Colour(0xff00bcd4));
        label->setJustificationType(juce::Justification::centred);

        label->onTextChange = [this, label]()
        {
            float typedPct = label->getText().getFloatValue();
            float newVal = juce::jlimit(0.0f, 1.0f, typedPct / 100.0f);
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
        if (auto* editor = label->getCurrentTextEditor())
            editor->grabKeyboardFocus();
    }

private:
    void setValue(float newValue)
    {
        if (ignoreCallbacks) return;
        currentValue = newValue;
        repaint();
    }

    juce::String sourceLabel;
    juce::ParameterAttachment attachment;
    float currentValue = 0.0f;
    bool dragging = false;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedbackGainCell)
};
