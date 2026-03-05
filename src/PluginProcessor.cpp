#include "PluginProcessor.h"
#include "PluginEditor.h"

ZeitraumProcessor::ZeitraumProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

ZeitraumProcessor::~ZeitraumProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
ZeitraumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    return layout;
}

const juce::String ZeitraumProcessor::getName() const { return "Zeitraum"; }
bool ZeitraumProcessor::acceptsMidi() const { return false; }
bool ZeitraumProcessor::producesMidi() const { return false; }
bool ZeitraumProcessor::isMidiEffect() const { return false; }
double ZeitraumProcessor::getTailLengthSeconds() const { return 0.0; }
int ZeitraumProcessor::getNumPrograms() { return 1; }
int ZeitraumProcessor::getCurrentProgram() { return 0; }
void ZeitraumProcessor::setCurrentProgram(int) {}
const juce::String ZeitraumProcessor::getProgramName(int) { return {}; }
void ZeitraumProcessor::changeProgramName(int, const juce::String&) {}

void ZeitraumProcessor::prepareToPlay(double, int) {}

void ZeitraumProcessor::releaseResources() {}

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

    // Clear any extra output channels beyond what we process
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Pass-through: input buffer is already the output buffer, nothing to do
}

bool ZeitraumProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ZeitraumProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
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
    xml->setAttribute("pluginVersion", 1);
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
