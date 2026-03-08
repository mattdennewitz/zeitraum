#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TapLevelFader : public juce::Component
{
public:
    explicit TapLevelFader(juce::RangedAudioParameter& levelParam)
        : attachment(levelParam,
                     [this](float newValue) { setValue(newValue); },
                     nullptr)
    {
        setOpaque(false);
        attachment.sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto barHeight = bounds.getHeight();

        // Background track
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRoundedRectangle(bounds, 2.0f);

        // Fader fill from bottom up
        float fillHeight = currentValue * barHeight;
        float minFillHeight = 2.0f;
        float displayFillHeight = juce::jmax(fillHeight, minFillHeight);

        auto fillRect = juce::Rectangle<float>(
            bounds.getX(), bounds.getBottom() - displayFillHeight,
            bounds.getWidth(), displayFillHeight);

        // Dim if value is zero
        float alpha = (currentValue < 0.01f) ? 0.3f : 1.0f;
        g.setColour(juce::Colour(0xff00bcd4).withAlpha(alpha));
        g.fillRoundedRectangle(fillRect, 2.0f);

        // Percentage text always visible
        int pct = static_cast<int>(currentValue * 100.0f + 0.5f);
        juce::String pctText = juce::String(pct) + "%";

        g.setColour(juce::Colour(0xffffffff).withAlpha(0.9f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(pctText, bounds.toNearestInt(), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        attachment.beginGesture();
        dragging = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;

        // Inverted: top = 1.0, bottom = 0.0
        float newValue = 1.0f - (e.position.y / static_cast<float>(getHeight()));
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

    juce::ParameterAttachment attachment;
    float currentValue = 0.0f;
    bool dragging = false;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapLevelFader)
};
