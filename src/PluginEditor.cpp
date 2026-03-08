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

    // Section labels
    auto setupSectionLabel = [this](juce::Label& l)
    {
        l.setFont(juce::FontOptions(12.0f));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        l.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
    };
    setupSectionLabel(delayTimeLabel);
    setupSectionLabel(levelLabel);
    setupSectionLabel(feedbackLabel);

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
    setResizeLimits(700, 440, 1400, 960);
    setSize(900, 560);
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

    // Left 60%: tap columns with section labels
    auto leftArea = mainArea.removeFromLeft(juce::roundToInt(mainArea.getWidth() * 0.6f));

    const int labelHeight = 16;
    const int numberHeight = 20;
    const int labelGap = 2;

    // "Delay Time" label
    delayTimeLabel.setBounds(leftArea.removeFromTop(labelHeight));
    leftArea.removeFromTop(labelGap);

    // Number labels at bottom
    auto numberRow = leftArea.removeFromBottom(numberHeight);
    leftArea.removeFromBottom(2); // gap above numbers

    // "Level" label + fader area at bottom of remaining
    // Fader = 25% of what TapColumn would compute, but we control it here
    int faderHeight = juce::roundToInt(leftArea.getHeight() * 0.20f);
    auto faderZone = leftArea.removeFromBottom(faderHeight);
    auto levelLabelRow = leftArea.removeFromBottom(labelHeight);
    leftArea.removeFromBottom(labelGap);
    levelLabel.setBounds(levelLabelRow);

    // Position bars fill the remaining space
    auto posBarZone = leftArea;

    // Now lay out 8 columns across each zone
    int gap = 3;
    int colWidth = (posBarZone.getWidth() - gap * 7) / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto posCol = posBarZone.removeFromLeft(colWidth);
        auto faderCol = faderZone.removeFromLeft(colWidth);
        auto numCol = numberRow.removeFromLeft(colWidth);

        // TapColumn bounds = union of all three zones for this column
        auto colUnion = posCol.getUnion(faderCol).getUnion(numCol);
        tapColumns[static_cast<size_t>(i)]->setBounds(colUnion);

        // Sub-component bounds relative to TapColumn's top-left
        tapColumns[static_cast<size_t>(i)]->setPositionBarBounds(posCol - colUnion.getPosition());
        tapColumns[static_cast<size_t>(i)]->setLevelFaderBounds(faderCol - colUnion.getPosition());
        tapColumns[static_cast<size_t>(i)]->setNumberLabelBounds(numCol - colUnion.getPosition());

        if (i < 7)
        {
            posBarZone.removeFromLeft(gap);
            faderZone.removeFromLeft(gap);
            numberRow.removeFromLeft(gap);
        }
    }

    // Right panel: remaining space
    auto rightArea = mainArea;
    rightArea.removeFromLeft(8); // gap between panels

    // "Feedback" label
    feedbackLabel.setBounds(rightArea.removeFromTop(labelHeight));
    rightArea.removeFromTop(labelGap);

    // Tap preset row at top of right panel
    auto presetRow = rightArea.removeFromTop(28);
    savePresetButton.setBounds(presetRow.removeFromRight(50));
    presetRow.removeFromRight(4);
    presetSelector.setBounds(presetRow);

    rightArea.removeFromTop(4);

    // Feedback matrix fills the rest
    feedbackMatrix.setBounds(rightArea);
}
