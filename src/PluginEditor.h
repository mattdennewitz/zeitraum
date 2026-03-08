#pragma once
#include "PluginProcessor.h"
#include "ui/ZeitraumLookAndFeel.h"
#include "ui/TopBar.h"
#include "ui/TapColumn.h"

class ZeitraumEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZeitraumEditor(ZeitraumProcessor&);
    ~ZeitraumEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // LookAndFeel MUST be first member (destroyed last -- see Pitfall 1)
    ZeitraumLookAndFeel lookAndFeel;

    ZeitraumProcessor& processorRef;
    TopBar topBar;
    std::array<std::unique_ptr<TapColumn>, 8> tapColumns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZeitraumEditor)
};
