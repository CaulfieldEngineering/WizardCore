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
        ),
        parameters(*this, nullptr, "PARAMETERS", {
            // LFO Parameters for testing and oscilloscope viewing
            std::make_unique<juce::AudioParameterFloat>(
                "lfo_frequency",           // parameterID
                "LFO Frequency",           // parameter name
                juce::NormalisableRange<float>(0.1f, 1000.0f, 0.1f, 0.3f), // range with skew
                1.0f,                      // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("Hz")
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "lfo_depth",               // parameterID
                "LFO Depth",               // parameter name
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                1.0f,                      // default value (full depth for proper sine wave)
                juce::AudioParameterFloatAttributes()
                    .withLabel("")
            ),
            
            std::make_unique<juce::AudioParameterBool>(
                "lfo_enabled",             // parameterID
                "LFO Enabled",             // parameter name
                true,                      // default value
                juce::AudioParameterBoolAttributes()
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "lfo_output_level",        // parameterID
                "LFO Output Level",        // parameter name
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                0.8f,                      // default value (80% for good oscilloscope viewing)
                juce::AudioParameterFloatAttributes()
                    .withLabel("")
            )
        })
    {
        // Initialize parameter pointers for quick access
        lfoFrequencyParam = parameters.getRawParameterValue("lfo_frequency");
        lfoDepthParam = parameters.getRawParameterValue("lfo_depth");
        lfoEnabledParam = parameters.getRawParameterValue("lfo_enabled");
        lfoOutputLevelParam = parameters.getRawParameterValue("lfo_output_level");
        
        DBG("PluginProcessor: LFO parameters initialized");
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

    double AudioPluginAudioProcessor::getTailLengthSeconds() const { 
        // Return appropriate tail length for the plugin
        return 0.0; 
    }

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
        DBG("prepareToPlay called with sample rate: " << sampleRate 
            << ", samples per block: " << samplesPerBlock);
        
        if (sampleRate <= 0.0) {
            DBG("Invalid sample rate, returning");
            return;
        }
        
        // Prepare LFO
        lfo.prepare(sampleRate);
        
        // Set initial LFO parameters
        lfo.setFrequency(lfoFrequencyParam->load());
        lfo.setDepth(lfoDepthParam->load());
        
        DBG("Plugin prepared successfully with LFO");
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
        const int numSamples = buffer.getNumSamples();
        
        // Clear any output channels that don't have corresponding input
        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear(i, 0, numSamples);
        
        // Safety check
        if (totalNumInputChannels == 0) {
            DBG("No input channels");
            return;
        }
        
        // Update LFO parameters from UI (only when they change)
        float newFrequency = lfoFrequencyParam->load();
        float newDepth = lfoDepthParam->load();
        
        // Only update if values have changed
        static float lastFrequency = -1.0f;
        static float lastDepth = -1.0f;
        
        if (newFrequency != lastFrequency) {
            lfo.setFrequency(newFrequency);
            lastFrequency = newFrequency;
        }
        
        if (newDepth != lastDepth) {
            lfo.setDepth(newDepth);
            lastDepth = newDepth;
        }
        
        // Process LFO and output to audio for oscilloscope viewing
        if (lfoEnabledParam->load() > 0.5f) {
            const float outputLevel = lfoOutputLevelParam->load();
            
            // Generate LFO signal once per sample, then apply to all channels
            for (int i = 0; i < numSamples; ++i) {
                // Get LFO sample ONCE per sample and scale by output level
                float lfoSample = lfo.getNextSample() * outputLevel;
                
                // Apply to all channels
                for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
                    float* channelData = buffer.getWritePointer(ch);
                    
                    // Mix with input signal (if any) or output LFO directly
                    if (totalNumInputChannels > 0) {
                        // Mix input with LFO signal
                        channelData[i] = channelData[i] + lfoSample;
                    } else {
                        // Output LFO signal directly (for oscilloscope viewing)
                        channelData[i] = lfoSample;
                    }
                }
                
                // Debug output (every 100 samples to avoid spam)
                static int debugCount = 0;
                if (++debugCount % 100 == 0) {
                    DBG("LFO pos:" << lfo.getPosition() << "\tsample:" << lfoSample);
                }
            }
        } else {
            // LFO disabled - just pass through input signal
            if (totalNumInputChannels > 0) {
                // Copy input to output
                for (int ch = 0; ch < std::min(totalNumInputChannels, totalNumOutputChannels); ++ch) {
                    buffer.copyFrom(ch, ch, 0, 0, numSamples);
                }
            }
        }
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
        // Use the APVTS to handle state saving
        auto state = parameters.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
        
        DBG("State saved");
    }

    void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        // Use the APVTS to handle state restoration
        std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        
        if (xmlState.get() != nullptr)
        {
            if (xmlState->hasTagName(parameters.state.getType()))
            {
                parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
                DBG("State restored");
            }
        }
    }
} // namespace audio_plugin

// This creates new instances of the plugin.
// This function definition must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new audio_plugin::AudioPluginAudioProcessor();
}