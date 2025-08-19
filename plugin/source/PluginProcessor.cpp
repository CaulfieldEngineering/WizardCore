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
            // Global Chorus parameters
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_rate",              // parameterID
                "Chorus Rate",              // parameter name
                juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f, 0.5f), // range with skew
                0.8f,                      // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("Hz")
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_depth",             // parameterID
                "Chorus Depth",             // parameter name
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                0.5f,                      // default value
                juce::AudioParameterFloatAttributes()
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_mix",               // parameterID
                "Chorus Mix",               // parameter name
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                0.5f,                      // default value
                juce::AudioParameterFloatAttributes()
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_base_delay",        // parameterID
                "Chorus Base Delay",        // parameter name
                juce::NormalisableRange<float>(10.0f, 100.0f, 1.0f),
                30.0f,                     // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("ms")
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_voice_count",       // parameterID
                "Number of Voices",         // parameter name
                juce::NormalisableRange<float>(1.0f, MAX_CHORUS_VOICES, 1.0f), // 1.0 to 5.0, step 1.0 (max supported by this instance)
                1.0f                        // default value
            ),
            
            std::make_unique<juce::AudioParameterBool>(
                "chorus_enabled",           // parameterID
                "Chorus Enabled",           // parameter name
                true                        // default value
            ),
            
            // Stereo Chorus parameters
            std::make_unique<juce::AudioParameterChoice>(
                "chorus_stereo_mode",       // parameterID
                "Stereo Mode",              // parameter name
                juce::StringArray{"Mono", "Stereo", "MidSide"}, // choices
                0                           // default value (Mono)
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_stereo_spread",     // parameterID
                "Stereo Spread",            // parameter name
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                0.5f                        // default value
            ),
            
            // Mid-Side Chorus parameters
            std::make_unique<juce::AudioParameterBool>(
                "chorus_mid_enabled",       // parameterID
                "Mid Enabled",              // parameter name
                true                        // default value
            ),
            
            std::make_unique<juce::AudioParameterBool>(
                "chorus_side_enabled",      // parameterID
                "Side Enabled",             // parameter name
                true                        // default value
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_side_gain",         // parameterID
                "Side Gain",                // parameter name
                juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
                0.0f,                      // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("dB")
            )
        }),
        chorus(MAX_CHORUS_VOICES)  // Initialize chorus with MAX_CHORUS_VOICES voices maximum
    {
        // Initialize global parameter pointers for quick access
        chorusRateParam = parameters.getRawParameterValue("chorus_rate");
        chorusDepthParam = parameters.getRawParameterValue("chorus_depth");
        chorusMixParam = parameters.getRawParameterValue("chorus_mix");
        chorusBaseDelayParam = parameters.getRawParameterValue("chorus_base_delay");
        chorusVoiceCountParam = parameters.getRawParameterValue("chorus_voice_count");
        chorusEnabledParam = parameters.getRawParameterValue("chorus_enabled");
        
        // Initialize stereo parameter pointers
        chorusStereoModeParam = parameters.getRawParameterValue("chorus_stereo_mode");
        chorusStereoSpreadParam = parameters.getRawParameterValue("chorus_stereo_spread");
        
        // Initialize mid-side parameter pointers
        chorusMidEnabledParam = parameters.getRawParameterValue("chorus_mid_enabled");
        chorusSideEnabledParam = parameters.getRawParameterValue("chorus_side_enabled");
        chorusSideGainParam = parameters.getRawParameterValue("chorus_side_gain");
        
        // Verify global parameter initialization
        if (!chorusRateParam || !chorusDepthParam || !chorusMixParam || !chorusBaseDelayParam || !chorusVoiceCountParam || !chorusEnabledParam) {
            DBG("PluginProcessor: Warning - Some global chorus parameters failed to initialize");
        }
        
        // Verify stereo parameter initialization
        if (!chorusStereoModeParam || !chorusStereoSpreadParam) {
            DBG("PluginProcessor: Warning - Some stereo chorus parameters failed to initialize");
        }
        
        // Verify mid-side parameter initialization
        if (!chorusMidEnabledParam || !chorusSideEnabledParam || !chorusSideGainParam) {
            DBG("PluginProcessor: Warning - Some mid-side chorus parameters failed to initialize");
        }
        
        DBG("PluginProcessor: Multi-voice Chorus parameters initialized");
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
        
        // Prepare Chorus - all parameter initialization will happen automatically
        // when updateParameters is called for the first time
        chorus.prepare(sampleRate, getTotalNumInputChannels());
        
        // Prepare test LFO for UI development
        testLFO.prepare(sampleRate);
        testLFO.setFrequency(1.0);  // 1 Hz default
        testLFO.setDepth(0.8f);     // 80% depth
        testLFO.setEnabled(true);   // Start enabled
		
        DBG("Plugin prepared successfully with Multi-Voice Chorus and Test LFO");
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

        // Process audio through the multi-voice chorus effect
        chorus.processBlock(buffer);
    }

    bool AudioPluginAudioProcessor::hasEditor() const {
        return true; // (change this to false if you choose to not supply an editor)
    }

    juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
        return new AudioPluginAudioProcessorEditor(*this);
        // return new juce::GenericAudioProcessorEditor(*this);  // Generic UI (commented out)
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