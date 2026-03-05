#pragma once
#include "PluginProcessor.h"

class ZeitraumEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZeitraumEditor(ZeitraumProcessor&);
    ~ZeitraumEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ZeitraumProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZeitraumEditor)
};
