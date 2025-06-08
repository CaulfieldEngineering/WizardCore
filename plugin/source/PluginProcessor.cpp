#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace audio_plugin {
    AudioPluginAudioProcessor::AudioPluginAudioProcessor()
        : AudioProcessor(
            BusesProperties()
    #if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
    #endif
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    #endif
        ) {
    }

    AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

    const juce::String AudioPluginAudioProcessor::getName() const {
    return JucePlugin_Name;
    }

    bool AudioPluginAudioProcessor::acceptsMidi() const {
    #if JucePlugin_WantsMidiInput
    return true;
    #else
    return false;
    #endif
    }

    bool AudioPluginAudioProcessor::producesMidi() const {
    #if JucePlugin_ProducesMidiOutput
    return true;
    #else
    return false;
    #endif
    }

    bool AudioPluginAudioProcessor::isMidiEffect() const {
    #if JucePlugin_IsMidiEffect
    return true;
    #else
    return false;
    #endif
    }

    double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

    int AudioPluginAudioProcessor::getNumPrograms() {
    return 1; // NB: some hosts don't cope very well if you tell them there are 0
                // programs, so this should be at least 1, even if you're not really
                // implementing programs.
    }

    int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

    void AudioPluginAudioProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
    }

    const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
    }

    void AudioPluginAudioProcessor::changeProgramName(int index,
                                                    const juce::String &newName) {
    juce::ignoreUnused(index, newName);
    }

    void AudioPluginAudioProcessor::prepareToPlay(double sampleRate,
                                                int samplesPerBlock) {
        DBG("prepareToPlay called with sample rate: " << sampleRate);
        
        if (sampleRate <= 0.0) {
            DBG("Invalid sample rate, returning");
            return;
        }
        
        DBG("Preparing delay line");
        delayLine.prepare(sampleRate, 2.0);
        DBG("Setting delay time");
        delayLine.setDelayTime(0.5);
        DBG("prepareToPlay completed");
    }

    void AudioPluginAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    }

    bool AudioPluginAudioProcessor::isBusesLayoutSupported(
        const BusesLayout &layouts) const {
    #if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
    #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
    #endif
    }

    void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                                juce::MidiBuffer &midiMessages) {
        juce::ignoreUnused(midiMessages);

        juce::ScopedNoDenormals noDenormals;
        auto totalNumInputChannels = getTotalNumInputChannels();
        auto totalNumOutputChannels = getTotalNumOutputChannels();

        DBG("processBlock: Input channels: " << totalNumInputChannels << ", Output channels: " << totalNumOutputChannels);

        // Clear any output channels that don't have input data
        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear(i, 0, buffer.getNumSamples());

        // Safety check - if no input channels, just return
        if (totalNumInputChannels == 0) {
            DBG("No input channels, returning");
            return;
        }

        // Process each channel
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            if (channelData == nullptr) {
                DBG("Null channel data for channel: " << channel);
                continue;
            }

            DBG("Processing channel: " << channel << " with " << buffer.getNumSamples() << " samples");
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float input = channelData[sample];
                float delayed = delayLine.process(input);
                channelData[sample] = delayed;
            }
        }
        DBG("processBlock completed");
    }

    bool AudioPluginAudioProcessor::hasEditor() const {
    	return true; // (change this to false if you choose to not supply an editor)
    }

    juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
    	// return new AudioPluginAudioProcessorEditor(*this);
    	return new juce::GenericAudioProcessorEditor(*this);
    }

    void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        // Create an XML element to store our parameters
        auto state = juce::XmlElement("PluginState");
        
        // Save the delay time
        state.setAttribute("delayTime", delayLine.getDelayTime());
        
        // Convert the XML to binary data
        copyXmlToBinary(state, destData);
    }

    void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        // Convert the binary data back to XML
        auto state = getXmlFromBinary(data, sizeInBytes);
        
        if (state != nullptr)
        {
            // Restore the delay time
            if (state->hasAttribute("delayTime"))
            {
                double delayTime = state->getDoubleAttribute("delayTime");
                delayLine.setDelayTime(delayTime);
            }
        }
    }
} // namespace audio_plugin

// This creates new instances of the plugin.
// This function definition must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new audio_plugin::AudioPluginAudioProcessor();
}