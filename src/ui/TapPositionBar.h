#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TapPositionBar : public juce::Component
{
public:
    TapPositionBar(juce::RangedAudioParameter& posParam,
                   juce::AudioProcessorValueTreeState& apvts)
        : apvtsRef(apvts),
          attachment(posParam,
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
        auto barWidth = bounds.getWidth();

        // Read APVTS params for ms display and grid (message thread safe)
        float baseDelay = 80.0f;
        float multiplier = 1.0f;
        bool quantizeOn = false;

        if (auto* p = apvtsRef.getParameter("BASE_DELAY"))
            baseDelay = p->convertFrom0to1(p->getValue());
        if (auto* p = apvtsRef.getParameter("MULTIPLIER"))
            multiplier = p->convertFrom0to1(p->getValue());
        if (auto* p = apvtsRef.getParameter("QUANTIZE"))
            quantizeOn = p->getValue() >= 0.5f;

        float maxMs = baseDelay * multiplier;

        // Background track
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRoundedRectangle(bounds, 2.0f);

        // Filled bar from bottom up to currentValue * height
        float fillHeight = currentValue * barHeight;
        float minFillHeight = 4.0f; // Always visible even at zero
        float displayFillHeight = juce::jmax(fillHeight, minFillHeight);

        auto fillRect = juce::Rectangle<float>(
            bounds.getX(), bounds.getBottom() - displayFillHeight,
            barWidth, displayFillHeight);

        // Dim if value is very low
        float alpha = (currentValue < 0.01f) ? 0.3f : 1.0f;
        g.setColour(juce::Colour(0xff00bcd4).withAlpha(alpha));
        g.fillRoundedRectangle(fillRect, 2.0f);

        // Draw horizontal drag handle line at bar top edge
        if (fillHeight > minFillHeight)
        {
            float handleY = bounds.getBottom() - fillHeight;
            g.setColour(juce::Colour(0xffffffff).withAlpha(0.8f));
            g.drawHorizontalLine(static_cast<int>(handleY), bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
        }

        // Quantize grid lines
        if (quantizeOn && maxMs > 0.0f)
        {
            g.setColour(juce::Colour(0xffffffff).withAlpha(0.15f));
            float stepMs = 10.0f;
            for (float ms = stepMs; ms < maxMs; ms += stepMs)
            {
                float normPos = ms / maxMs;
                float lineY = bounds.getBottom() - (normPos * barHeight);
                if (lineY > bounds.getY() && lineY < bounds.getBottom())
                    g.drawHorizontalLine(static_cast<int>(lineY),
                                         bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
            }
        }

        // Ms value label
        float ms = currentValue * maxMs;
        juce::String msText;
        if (ms >= 100.0f)
            msText = juce::String(static_cast<int>(ms)) + "ms";
        else
            msText = juce::String(ms, 1) + "ms";

        g.setColour(juce::Colour(0xffffffff).withAlpha(0.9f));
        g.setFont(juce::FontOptions(10.0f));

        // Draw ms text at bottom of bar area
        auto textRect = bounds.removeFromBottom(14.0f);
        g.drawText(msText, textRect.toNearestInt(), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        juce::ignoreUnused(e);
        dragging = true;
        attachment.beginGesture();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;

        // Inverted: top of component = 1.0, bottom = 0.0
        float newValue = 1.0f - (e.position.y / static_cast<float>(getHeight()));
        newValue = juce::jlimit(0.0f, 1.0f, newValue);

        currentValue = newValue;
        repaint();

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
        // Read current max ms for conversion
        float baseDelay = 80.0f;
        float multiplier = 1.0f;
        if (auto* p = apvtsRef.getParameter("BASE_DELAY"))
            baseDelay = p->convertFrom0to1(p->getValue());
        if (auto* p = apvtsRef.getParameter("MULTIPLIER"))
            multiplier = p->convertFrom0to1(p->getValue());
        float maxMs = baseDelay * multiplier;

        float currentMs = currentValue * maxMs;

        auto* label = new juce::Label();
        label->setEditable(true, true, false);
        label->setText(juce::String(currentMs, 1), juce::dontSendNotification);
        label->setBounds(getLocalBounds().reduced(2));
        label->setFont(juce::FontOptions(12.0f));
        label->setColour(juce::Label::backgroundColourId, juce::Colour(0xff252525));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        label->setColour(juce::Label::outlineColourId, juce::Colour(0xff00bcd4));
        label->setJustificationType(juce::Justification::centred);

        float capturedMaxMs = maxMs;
        label->onTextChange = [this, label, capturedMaxMs]()
        {
            float typedMs = label->getText().getFloatValue();
            float newPos = (capturedMaxMs > 0.0f) ? (typedMs / capturedMaxMs) : 0.0f;
            newPos = juce::jlimit(0.0f, 1.0f, newPos);
            attachment.setValueAsCompleteGesture(newPos);
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

    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::ParameterAttachment attachment;
    float currentValue = 0.0f;
    bool dragging = false;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapPositionBar)
};
