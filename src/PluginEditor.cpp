#include "PluginEditor.h"

ZeitraumEditor::ZeitraumEditor(ZeitraumProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      topBar(p.apvts),
      feedbackMatrix(p.apvts)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(topBar);

    for (int i = 0; i < 8; ++i)
    {
        tapColumns[static_cast<size_t>(i)] = std::make_unique<TapColumn>(i, processorRef.apvts);
        addAndMakeVisible(*tapColumns[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(feedbackMatrix);

    // Tap preset selector
    auto presetNames = processorRef.getTapPresetNames();
    presetSelector.addItem("-- Presets --", 1);
    for (int i = 0; i < presetNames.size(); ++i)
        presetSelector.addItem(presetNames[i], i + 2);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this]()
    {
        int selectedId = presetSelector.getSelectedId();
        if (selectedId > 1)
        {
            juce::String name = presetSelector.getText();
            processorRef.recallTapPreset(name);
        }
    };
    addAndMakeVisible(presetSelector);

    // Save preset button
    savePresetButton.onClick = [this]()
    {
        auto* aw = new juce::AlertWindow("Save Tap Preset",
                                          "Enter a name for the tap preset:",
                                          juce::MessageBoxIconType::QuestionIcon);
        aw->addTextEditor("presetName", "", "Name:");
        aw->addButton("Save", 1);
        aw->addButton("Cancel", 0);

        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw](int result)
            {
                if (result == 1)
                {
                    juce::String name = aw->getTextEditorContents("presetName").trim();
                    if (name.isNotEmpty())
                    {
                        processorRef.saveTapPreset(name);

                        // Repopulate preset selector
                        presetSelector.clear();
                        presetSelector.addItem("-- Presets --", 1);
                        auto names = processorRef.getTapPresetNames();
                        for (int i = 0; i < names.size(); ++i)
                            presetSelector.addItem(names[i], i + 2);

                        // Select the newly saved preset
                        int newIdx = names.indexOf(name);
                        if (newIdx >= 0)
                            presetSelector.setSelectedId(newIdx + 2, juce::dontSendNotification);
                    }
                }
                delete aw;
            }), true);
    };
    addAndMakeVisible(savePresetButton);

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

    // Right panel: remaining space
    auto rightArea = mainArea;
    rightArea.removeFromLeft(8); // gap between panels

    // Tap preset row at top of right panel
    auto presetRow = rightArea.removeFromTop(28);
    savePresetButton.setBounds(presetRow.removeFromRight(50));
    presetRow.removeFromRight(4);
    presetSelector.setBounds(presetRow);

    rightArea.removeFromTop(4);

    // Feedback matrix fills the rest
    feedbackMatrix.setBounds(rightArea);
}
