#include "PluginEditor.h"

ZeitraumEditor::ZeitraumEditor(ZeitraumProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setSize(400, 300);
}

ZeitraumEditor::~ZeitraumEditor() {}

void ZeitraumEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF2D2D2D));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f));
    g.drawFittedText("Zeitraum", getLocalBounds(), juce::Justification::centred, 1);
}

void ZeitraumEditor::resized()
{
    // No child components yet
}
