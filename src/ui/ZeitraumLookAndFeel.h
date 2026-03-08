#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

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
        // Slider colours
        setColour(juce::Slider::trackColourId, juce::Colour(0xff00bcd4));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3a3a3a));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcccccc));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff252525));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));

        // ComboBox colours
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff3a3a3a));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffcccccc));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff555555));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff00bcd4));

        // PopupMenu colours
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff252525));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffcccccc));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff00acc1));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));

        // ToggleButton colours
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00bcd4));

        // Label colours
        setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
        {
            auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                  static_cast<float>(width), static_cast<float>(height));

            // Background
            g.setColour(juce::Colour(0xff3a3a3a));
            g.fillRoundedRectangle(bounds, 3.0f);

            // Fill bar
            float fillWidth = sliderPos - static_cast<float>(x);
            if (fillWidth > 0.0f)
            {
                auto fillRect = bounds.withWidth(fillWidth);
                g.setColour(juce::Colour(0xff00bcd4));
                g.fillRoundedRectangle(fillRect, 3.0f);
            }

            // Value text
            g.setColour(juce::Colour(0xffffffff));
            g.setFont(juce::FontOptions(12.0f));
            g.drawText(slider.getTextFromValue(slider.getValue()),
                       bounds.toNearestInt(), juce::Justification::centred, false);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                                    sliderPos, 0.0f, 0.0f, style, slider);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat();
        auto tickBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);

        // Toggle box background
        g.setColour(button.getToggleState() ? juce::Colour(0xff00bcd4) : juce::Colour(0xff3a3a3a));
        g.fillRoundedRectangle(tickBounds, 3.0f);

        // Outline on hover
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xff00acc1));
            g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);
        }

        // Check mark when toggled
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(0xffffffff));
            auto tick = tickBounds.reduced(3.0f);
            juce::Path checkPath;
            checkPath.startNewSubPath(tick.getX(), tick.getCentreY());
            checkPath.lineTo(tick.getCentreX(), tick.getBottom());
            checkPath.lineTo(tick.getRight(), tick.getY());
            g.strokePath(checkPath, juce::PathStrokeType(2.0f));
        }

        // Button text
        g.setColour(juce::Colour(0xffcccccc));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(button.getButtonText(), bounds.toNearestInt(),
                   juce::Justification::centredLeft, true);
    }

    void drawComboBox(juce::Graphics& g, int width, int height,
                      bool /*isButtonDown*/, int /*buttonX*/, int /*buttonY*/,
                      int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                              static_cast<float>(width),
                                              static_cast<float>(height));

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 3.0f);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

        // Arrow
        auto arrowZone = bounds.removeFromRight(static_cast<float>(height)).reduced(8.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getY(),
                          arrowZone.getRight(), arrowZone.getY(),
                          arrowZone.getCentreX(), arrowZone.getBottom());
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        juce::ignoreUnused(isSeparator, isActive, isTicked, hasSubMenu,
                           shortcutKeyText, icon, textColour);

        if (isHighlighted)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(area);
            g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour(findColour(juce::PopupMenu::textColourId));
        }

        g.setFont(juce::FontOptions(13.0f));
        g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft, true);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZeitraumLookAndFeel)
};
