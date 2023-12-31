/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChorusAudioProcessor::ChorusAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
    PropertiesFile::Options options;
    options.applicationName = "Chorus-Plugin";
    options.folderName = "lachesis17";
    appProperties.setStorageParameters(options);
}

ChorusAudioProcessor::~ChorusAudioProcessor()
{
    // juce::File Log("**/build/Delay_artefacts/Debug/Standalone/feedback.txt"); // log making
    // juce::FileLogger Logger(feedbackLog, "Log Message");
    // Logger.logMessage("Value: " + juce::String(delayTimeLeft));
}

//==============================================================================
const juce::String ChorusAudioProcessor::getName() const
{
    return "";
}

bool ChorusAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChorusAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChorusAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChorusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChorusAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChorusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChorusAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ChorusAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChorusAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ChorusAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec{sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2};

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    currentSampleRate = getSampleRate();

    coeff = 1.0f - std::exp( -1.0f / (0.1f * currentSampleRate)); // tape delay effect : one-pole filter
    coeff_chrs = 1.0f - std::exp( -1.0f / (0.01f * currentSampleRate));

    smoothedDelayTimeLeft.reset(currentSampleRate, 0.3f);
    smoothedDelayTimeRight.reset(currentSampleRate, 0.3f);
    smoothedChorusDepth.reset(currentSampleRate, 0.005);
    smoothedChorusRate.reset(currentSampleRate, 0.005);

    circBuffLeft.createCircularBuffer(2 * currentSampleRate);   // doubled or limited to 1365ms @ 48k
    circBuffRight.createCircularBuffer(2 * currentSampleRate);
    circBuffLeft.flushBuffer();
    circBuffRight.flushBuffer();
}


void ChorusAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    auto chainsettings = getChainSettings(apvts);
    float dryWet = 1.f;
    bool dualDelay = chainsettings.dualDelay;
    bool chorus = chainsettings.chorus;
    float newChorusDepth = chainsettings.depth;
    float newChorusRate = chainsettings.rate;
    float newDelayTimeLeft = chainsettings.delayTimeLeft;
    float newDelayTimeRight = dualDelay ? chainsettings.delayTimeRight : chainsettings.delayTimeLeft;

    smoothedChorusDepth.setTargetValue(newChorusDepth);
    chorusDepth = smoothedChorusDepth.getNextValue() + ((smoothedChorusDepth.getNextValue() - chorusDepth) * coeff_chrs); 
    smoothedChorusRate.setTargetValue(newChorusRate);
    chorusRate = smoothedChorusRate.getNextValue() + ((smoothedChorusRate.getNextValue() - chorusRate) * coeff_chrs); 

    for (int channel = 0; channel < numChannels; ++channel)
    {
    const float* inData = buffer.getReadPointer(channel);
    float* outData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {

            if (channel == 0) // left channel
            {
                smoothedDelayTimeLeft.setTargetValue(newDelayTimeLeft);
                delayTimeLeft = smoothedDelayTimeLeft.getNextValue() + ((smoothedDelayTimeLeft.getNextValue() - delayTimeLeft) * coeff); //delayTimeLeft += (newDelayTimeLeft - delayTimeLeft) * coeff; // non-smoothed, apply one-pole filter

                if (chorus && (delayTimeLeft != 0.0f))
                    applyChorus(sample, true);

                float delayedSample = circBuffLeft.readBuffer(delayTimeLeft * currentSampleRate / 1000.0);
                circBuffLeft.writeBuffer(inData[sample] + 0 * delayedSample);
                float wetScale = (1.0f - dryWet) + dryWet * 0.5;  // making this to control the volume changes when mixing dry/wet signals
                outData[sample] = wetScale * inData[sample] + dryWet * delayedSample;  // dry / wet   //outData[sample] = delayedSample; // 100% wet  // outData[sample] = (1.0f - dryWet) * inData[sample] + dryWet * delayedSample; // original
                writeIndexLeft = (writeIndexLeft + 1) % circBuffLeft.getBufferLength();
            }
            else if (channel == 1) // right channel
            {
                smoothedDelayTimeRight.setTargetValue(newDelayTimeRight);
                delayTimeRight = smoothedDelayTimeRight.getNextValue() + ((smoothedDelayTimeRight.getNextValue() - delayTimeRight) * coeff);

                if (chorus && (delayTimeRight != 0.0f))
                    applyChorus(sample, false);

                float delayedSample = circBuffRight.readBuffer(delayTimeRight * currentSampleRate / 1000.0);
                circBuffRight.writeBuffer(inData[sample] + 0 * delayedSample);
                float wetScale = (1.0f - dryWet) + dryWet * 0.5;
                outData[sample] = wetScale * inData[sample] + dryWet * delayedSample; 
                writeIndexRight = (writeIndexRight + 1) % circBuffRight.getBufferLength();
            }
        }
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    leftChain.process(leftContext);
    rightChain.process(rightContext);

    lastDelayTimeLeft = newDelayTimeLeft;
    lastDelayTimeRight = newDelayTimeRight;
}


//==============================================================================
bool ChorusAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ChorusAudioProcessor::createEditor()
{
    return new ChorusAudioProcessorEditor (*this);
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    //auto editor = apvts.state.getOrCreateChildWithName ("editor", nullptr);
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void ChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.delayTimeLeft = apvts.getRawParameterValue("Delay Left")->load();
    settings.delayTimeRight = apvts.getRawParameterValue("Delay Right")->load();
    settings.depth = apvts.getRawParameterValue("Depth")->load();
    settings.rate = apvts.getRawParameterValue("Rate")->load();
    settings.dualDelay = apvts.getRawParameterValue("Dual Delay")->load() < 0.5f;
    settings.chorus = apvts.getRawParameterValue("Chorus")->load() < 0.5f;

    return settings;
}

void ChorusAudioProcessor::applyChorus(int sample, bool left)
{
    chorusModulation = chorusDepth * std::sin(2.0 * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase);

    if (left) 
    {
        float newDelayTime = delayTimeLeft + chorusModulation;
        delayTimeLeft = newDelayTime + (newDelayTime - delayTimeLeft) * coeff;
    } 
    else 
    {
        float newDelayTime = delayTimeRight + chorusModulation;
        delayTimeRight = newDelayTime + (newDelayTime - delayTimeRight) * coeff;
    }

    chorusPhase += 2.0 * juce::MathConstants<float>::pi * chorusRate / currentSampleRate;

    while (chorusPhase >= 2.0 * juce::MathConstants<float>::pi)
        chorusPhase -= 2.0 * juce::MathConstants<float>::pi;
}

// float ChorusAudioProcessor::smoothValues(float current, juce::LinearSmoothedValue<float> smoothed, float next)
// { // need to fix this...
//     smoothed.setTargetValue(next);
//     current = smoothed.getNextValue() + ((smoothed.getNextValue() - current) * coeff); 
//     return current;
// }

void ChorusAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);

    // reserved for low and high pass
}


juce::AudioProcessorValueTreeState::ParameterLayout ChorusAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Left", "Delay Left", 5, 40, 25));
    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Right", "Delay Right", 5, 40, 15));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Depth", "Depth", juce::NormalisableRange<float>(0.f, 1.f, 0.01f, 1.f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Rate", "Rate", juce::NormalisableRange<float>(1.f, 5.f, 0.02f, 1.f), 1.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Dual Delay", "Dual Delay", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Chorus", "Chorus", false));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusAudioProcessor();
}
