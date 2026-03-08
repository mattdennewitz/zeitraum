#include "PluginEditor.h"

ZeitraumEditor::ZeitraumEditor(ZeitraumProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      topBar(p.apvts)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(topBar);

    setResizable(true, true);
    setResizeLimits(700, 400, 1400, 900);
    setSize(900, 500);
}

ZeitraumEditor::~ZeitraumEditor()
{
    setLookAndFeel(nullptr);
}

void ZeitraumEditor::paint(juce::Graphics& g)
{
    g.fillAll(lookAndFeel.getCurrentColourScheme()
                  .getUIColour(juce::LookAndFeel_V4::ColourScheme::windowBackground));

    auto bounds = getLocalBounds();

    // Separator line below top bar
    auto separatorY = 50;
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(separatorY, 0.0f, static_cast<float>(bounds.getWidth()));

    // Main content area
    auto mainArea = bounds.withTrimmedTop(separatorY + 1).reduced(8, 4);
    auto leftArea = mainArea.removeFromLeft(juce::roundToInt(mainArea.getWidth() * 0.6f));
    auto rightArea = mainArea;

    // Placeholder text for future panels
    g.setColour(juce::Colour(0xff555555));
    g.setFont(juce::FontOptions(16.0f));
    g.drawText("Tap Columns", leftArea, juce::Justification::centred, false);
    g.drawText("Feedback Matrix", rightArea, juce::Justification::centred, false);
}

void ZeitraumEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top bar: fixed height 50px
    topBar.setBounds(bounds.removeFromTop(50));
}
