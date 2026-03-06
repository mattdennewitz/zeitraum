#include "PluginProcessor.h"
#include "PluginEditor.h"

ZeitraumProcessor::ZeitraumProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    baseDelayParam = apvts.getRawParameterValue("BASE_DELAY");
    multiplierParam = apvts.getRawParameterValue("MULTIPLIER");
    mixParam = apvts.getRawParameterValue("MIX");
    characterParam = apvts.getRawParameterValue("CHARACTER");
    quantizeParam = apvts.getRawParameterValue("QUANTIZE");

    for (int i = 0; i < 8; ++i)
    {
        tapPosParams[i] = apvts.getRawParameterValue("TAP" + juce::String(i + 1) + "_POS");
        tapLevelParams[i] = apvts.getRawParameterValue("TAP" + juce::String(i + 1) + "_LEVEL");
    }

    // Cache feedback parameter pointers
    for (int i = 0; i < 8; ++i)
        fbTapGainParams[i] = apvts.getRawParameterValue("FB_TAP" + juce::String(i + 1));

    const juce::String mixNames[] = {"FB_ODD", "FB_EVEN", "FB_RISING", "FB_FALLING"};
    for (int i = 0; i < 4; ++i)
        fbMixGainParams[i] = apvts.getRawParameterValue(mixNames[i]);

    fbHPFreqParam = apvts.getRawParameterValue("FB_HP_FREQ");
    fbLPFreqParam = apvts.getRawParameterValue("FB_LP_FREQ");
    fbHPOnParam = apvts.getRawParameterValue("FB_HP_ON");
    fbLPOnParam = apvts.getRawParameterValue("FB_LP_ON");
}

ZeitraumProcessor::~ZeitraumProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
ZeitraumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Base delay time: 10-150ms, skewed toward lower values
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"BASE_DELAY", 1}, "Base Delay",
        juce::NormalisableRange<float>(10.0f, 150.0f, 0.1f, 0.5f),
        80.0f, "ms"));

    // Multiplier: 1x to 33x (max total = 150*33 = 4950ms ~5s)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MULTIPLIER", 1}, "Multiplier",
        juce::NormalisableRange<float>(1.0f, 33.0f, 0.01f, 0.4f),
        1.0f, "x"));

    // Wet/dry mix: 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MIX", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f, "%"));

    // Character: 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"CHARACTER", 1}, "Character",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        25.0f, "%"));

    // Quantize toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"QUANTIZE", 1}, "Quantize", false));

    // Per-tap parameters (8 taps)
    for (int i = 1; i <= 8; ++i)
    {
        auto id = juce::String(i);

        // Tap position: 0.0-1.0 ratio along delay line
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + id + "_POS", 1},
            "Tap " + id + " Position",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            static_cast<float>(i) / 8.0f));

        // Tap level: 0.0-1.0 linear gain
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"TAP" + id + "_LEVEL", 1},
            "Tap " + id + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            1.0f));
    }

    // Feedback gain parameters (8 individual taps + 4 preset mixes)
    for (int i = 1; i <= 8; ++i)
    {
        auto id = juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"FB_TAP" + id, 1},
            "FB Tap " + id,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
            0.0f, "%"));
    }

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_ODD", 1}, "FB Odd",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_EVEN", 1}, "FB Even",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_RISING", 1}, "FB Rising",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_FALLING", 1}, "FB Falling",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f, "%"));

    // Feedback filter parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_HP_FREQ", 1}, "FB HP Freq",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 0.1f, 0.3f),
        20.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"FB_LP_FREQ", 1}, "FB LP Freq",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 0.1f, 0.3f),
        20000.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"FB_HP_ON", 1}, "FB HP On", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"FB_LP_ON", 1}, "FB LP On", false));

    return layout;
}

const juce::String ZeitraumProcessor::getName() const { return "Zeitraum"; }
bool ZeitraumProcessor::acceptsMidi() const { return false; }
bool ZeitraumProcessor::producesMidi() const { return false; }
bool ZeitraumProcessor::isMidiEffect() const { return false; }
double ZeitraumProcessor::getTailLengthSeconds() const { return 150.0 * 33.0 / 1000.0; }
int ZeitraumProcessor::getNumPrograms() { return 1; }
int ZeitraumProcessor::getCurrentProgram() { return 0; }
void ZeitraumProcessor::setCurrentProgram(int) {}
const juce::String ZeitraumProcessor::getProgramName(int) { return {}; }
void ZeitraumProcessor::changeProgramName(int, const juce::String&) {}

void ZeitraumProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    delayEngine.prepare(sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;
    dryWetMixer.prepare(spec);
    dryWetMixer.setWetMixProportion(mixParam->load() / 100.0f);
}

void ZeitraumProcessor::releaseResources()
{
    delayEngine.reset();
    dryWetMixer.reset();
}

bool ZeitraumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void ZeitraumProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();

    // Update DryWetMixer proportion (smoothed internally by DryWetMixer)
    dryWetMixer.setWetMixProportion(mixParam->load() / 100.0f);

    // Push dry samples into DryWetMixer before processing
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Load parameter values atomically
    float baseDelay = baseDelayParam->load();
    float multiplier = multiplierParam->load();
    float character = characterParam->load() / 100.0f;
    bool quantize = quantizeParam->load() > 0.5f;

    // Build tap position and level arrays from cached param pointers
    float tapPositions[8];
    float tapLevels[8];
    for (int i = 0; i < 8; ++i)
    {
        tapPositions[i] = tapPosParams[i]->load();
        tapLevels[i] = tapLevelParams[i]->load();
    }

    // Build feedback gain array (12 values: 8 taps + 4 preset mixes)
    float feedbackGains[12];
    for (int i = 0; i < 8; ++i)
        feedbackGains[i] = fbTapGainParams[i]->load() / 100.0f;
    for (int i = 0; i < 4; ++i)
        feedbackGains[8 + i] = fbMixGainParams[i]->load() / 100.0f;

    float fbHPFreq = fbHPFreqParam->load();
    float fbLPFreq = fbLPFreqParam->load();
    bool fbHPOn = fbHPOnParam->load() > 0.5f;
    bool fbLPOn = fbLPOnParam->load() > 0.5f;

    // Process delay engine with feedback (writes wet signal into buffer)
    delayEngine.process(buffer, baseDelay, multiplier, character, quantize,
                        tapPositions, tapLevels, feedbackGains,
                        fbHPFreq, fbLPFreq, fbHPOn, fbLPOn);

    // Mix wet with dry via DryWetMixer
    dryWetMixer.mixWetSamples(block);

    // Clear any extra output channels beyond what we process
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

bool ZeitraumProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ZeitraumProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void ZeitraumProcessor::saveTapPreset(const juce::String& name)
{
    auto presetsNode = apvts.state.getOrCreateChildWithName("TapPresets", nullptr);

    // Remove existing preset with same name
    for (int i = presetsNode.getNumChildren() - 1; i >= 0; --i)
    {
        if (presetsNode.getChild(i).getProperty("name").toString() == name)
            presetsNode.removeChild(i, nullptr);
    }

    juce::ValueTree preset("TapPreset");
    preset.setProperty("name", name, nullptr);

    for (int i = 1; i <= 8; ++i)
    {
        auto paramId = "TAP" + juce::String(i) + "_POS";
        auto* param = apvts.getRawParameterValue(paramId);
        if (param != nullptr)
            preset.setProperty(juce::Identifier(paramId), param->load(), nullptr);
    }

    presetsNode.appendChild(preset, nullptr);
}

void ZeitraumProcessor::recallTapPreset(const juce::String& name)
{
    auto presetsNode = apvts.state.getChildWithName("TapPresets");
    if (!presetsNode.isValid())
        return;

    for (int i = 0; i < presetsNode.getNumChildren(); ++i)
    {
        auto preset = presetsNode.getChild(i);
        if (preset.getProperty("name").toString() == name)
        {
            for (int t = 1; t <= 8; ++t)
            {
                auto paramId = "TAP" + juce::String(t) + "_POS";
                if (preset.hasProperty(juce::Identifier(paramId)))
                {
                    float value = static_cast<float>(preset.getProperty(juce::Identifier(paramId)));
                    if (auto* param = apvts.getParameter(paramId))
                        param->setValueNotifyingHost(param->convertTo0to1(value));
                }
            }
            return;
        }
    }
}

juce::StringArray ZeitraumProcessor::getTapPresetNames() const
{
    juce::StringArray names;
    auto presetsNode = apvts.state.getChildWithName("TapPresets");
    if (presetsNode.isValid())
    {
        for (int i = 0; i < presetsNode.getNumChildren(); ++i)
            names.add(presetsNode.getChild(i).getProperty("name").toString());
    }
    return names;
}

void ZeitraumProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr)
    {
        jassertfalse;
        return;
    }
    xml->setAttribute("pluginVersion", 2);
    copyXmlToBinary(*xml, destData);
}

void ZeitraumProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
    {
        DBG("Zeitraum: setStateInformation failed -- could not parse XML from "
            + juce::String(sizeInBytes) + " bytes");
        jassertfalse;
        return;
    }
    if (!xml->hasTagName(apvts.state.getType()))
    {
        DBG("Zeitraum: setStateInformation failed -- expected tag '"
            + apvts.state.getType().toString() + "' but got '" + xml->getTagName() + "'");
        jassertfalse;
        return;
    }

    // Version stored in XML for future migration (see getStateInformation)
    int version = xml->getIntAttribute("pluginVersion", 1);
    jassert(version >= 1);
    juce::ignoreUnused(version);

    auto newState = juce::ValueTree::fromXml(*xml);
    if (!newState.isValid())
    {
        DBG("Zeitraum: setStateInformation failed -- XML did not produce a valid ValueTree");
        jassertfalse;
        return;
    }
    apvts.replaceState(newState);
}

// Plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZeitraumProcessor();
}
