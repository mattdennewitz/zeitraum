#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/DelayEngine.h"

class ZeitraumProcessor : public juce::AudioProcessor
{
public:
    ZeitraumProcessor();
    ~ZeitraumProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Tap preset management
    void saveTapPreset(const juce::String& name);
    void recallTapPreset(const juce::String& name);
    juce::StringArray getTapPresetNames() const;

    void randomizeParameters();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    DelayEngine delayEngine;
    juce::dsp::DryWetMixer<float> dryWetMixer;

    // Cached parameter pointers (lock-free, realtime-safe)
    std::atomic<float>* baseDelayParam = nullptr;
    std::atomic<float>* multiplierParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* quantizeParam = nullptr;
    std::atomic<float>* tapPosParams[8] = {};
    std::atomic<float>* tapLevelParams[8] = {};

    // Feedback parameter cache pointers
    std::atomic<float>* fbTapGainParams[8] = {};
    std::atomic<float>* fbMixGainParams[4] = {};  // ODD, EVEN, RISING, FALLING
    std::atomic<float>* fbHPFreqParam = nullptr;
    std::atomic<float>* fbLPFreqParam = nullptr;
    std::atomic<float>* fbHPOnParam = nullptr;
    std::atomic<float>* fbLPOnParam = nullptr;

    // Output mix preset selector
    std::atomic<float>* outputMixParam = nullptr;
    juce::RangedAudioParameter* outputMixParamObj = nullptr;
    juce::RangedAudioParameter* tapLevelParamObjs[8] = {};

    // Tempo sync parameters
    std::atomic<float>* tempoSyncParam = nullptr;
    std::atomic<float>* noteDivParam = nullptr;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZeitraumProcessor)
};
