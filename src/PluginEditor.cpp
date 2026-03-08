#include "PluginEditor.h"

ZeitraumEditor::ZeitraumEditor(ZeitraumProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      topBar(p.apvts)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(topBar);

    for (int i = 0; i < 8; ++i)
    {
        tapColumns[static_cast<size_t>(i)] = std::make_unique<TapColumn>(i, processorRef.apvts);
        addAndMakeVisible(*tapColumns[static_cast<size_t>(i)]);
    }

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

    // Main content area -- right panel placeholder
    auto mainArea = bounds.withTrimmedTop(separatorY + 1).reduced(8, 4);
    auto leftArea = mainArea.removeFromLeft(juce::roundToInt(mainArea.getWidth() * 0.6f));
    juce::ignoreUnused(leftArea); // Tap columns positioned in resized()
    auto rightArea = mainArea;

    g.setColour(juce::Colour(0xff555555));
    g.setFont(juce::FontOptions(16.0f));
    g.drawText("Feedback Matrix", rightArea, juce::Justification::centred, false);
}

void ZeitraumEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top bar: fixed height 50px
    topBar.setBounds(bounds.removeFromTop(50));

    // Main content area with padding
    auto mainArea = bounds.reduced(8, 4);

    // Left 60%: tap columns
    auto leftArea = mainArea.removeFromLeft(juce::roundToInt(mainArea.getWidth() * 0.6f));

    // Divide left panel into 8 equal-width columns with gaps
    int gap = 3;
    int totalGaps = gap * 7;
    int colWidth = (leftArea.getWidth() - totalGaps) / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto colArea = leftArea.removeFromLeft(colWidth);
        tapColumns[static_cast<size_t>(i)]->setBounds(colArea);

        if (i < 7)
            leftArea.removeFromLeft(gap);
    }
}
