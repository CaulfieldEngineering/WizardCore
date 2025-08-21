#include "PluginProcessor.h"
#include "Chorus/Chorus.h"

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
            ),
            
            // LPF parameters
            std::make_unique<juce::AudioParameterBool>(
                "chorus_lpf_enabled",      // parameterID
                "LPF Enabled",             // parameter name
                false                      // default value (disabled)
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_lpf_cutoff",       // parameterID
                "LPF Cutoff",              // parameter name
                juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), // range with skew
                20000.0f,                  // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("Hz")
            ),
            
            // HPF parameters
            std::make_unique<juce::AudioParameterBool>(
                "chorus_hpf_enabled",      // parameterID
                "HPF Enabled",             // parameter name
                false                      // default value (disabled)
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "chorus_hpf_cutoff",       // parameterID
                "HPF Cutoff",              // parameter name
                juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), // range with skew
                20.0f,                     // default value
                juce::AudioParameterFloatAttributes()
                    .withLabel("Hz")
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
        
        // Initialize LPF parameter pointers
        chorusLPFEnabledParam = parameters.getRawParameterValue("chorus_lpf_enabled");
        chorusLPFCutoffParam = parameters.getRawParameterValue("chorus_lpf_cutoff");
        
        // Initialize HPF parameter pointers
        chorusHPFEnabledParam = parameters.getRawParameterValue("chorus_hpf_enabled");
        chorusHPFCutoffParam = parameters.getRawParameterValue("chorus_hpf_cutoff");
        
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
        
        // Verify filter parameter initialization
        if (!chorusLPFEnabledParam || !chorusLPFCutoffParam || !chorusHPFEnabledParam || !chorusHPFCutoffParam) {
            DBG("PluginProcessor: Warning - Some filter parameters failed to initialize");
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
        
        // Initialize chorus with default parameters
        chorus.setNumVoices(1);  // Start with 1 voice
        chorus.setMix(0.5f);     // 50% wet/dry mix
        chorus.setBaseDelay(30.0f); // 30ms base delay
		
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

        // Update chorus parameters from the AudioProcessorValueTreeState
        if (chorusEnabledParam && *chorusEnabledParam > 0.5f) {
            // Only process if chorus is enabled
            
            // Get all current parameter values
            float rate = chorusRateParam ? static_cast<float>(*chorusRateParam) : 0.8f;
            float depth = chorusDepthParam ? static_cast<float>(*chorusDepthParam) : 0.5f;
            float mix = chorusMixParam ? static_cast<float>(*chorusMixParam) : 0.5f;
            float baseDelay = chorusBaseDelayParam ? static_cast<float>(*chorusBaseDelayParam) : 30.0f;
            int voiceCount = chorusVoiceCountParam ? static_cast<int>(*chorusVoiceCountParam) : 1;
            
            // Convert stereo mode choice to integer (0=Mono, 1=Stereo, 2=MidSide)
            int stereoMode = 0; // Default to Mono
            if (chorusStereoModeParam) {
                stereoMode = static_cast<int>(*chorusStereoModeParam);
            }
            
            float stereoSpread = chorusStereoSpreadParam ? static_cast<float>(*chorusStereoSpreadParam) : 0.5f;
            bool midEnabled = chorusMidEnabledParam ? (static_cast<float>(*chorusMidEnabledParam) > 0.5f) : true;
            bool sideEnabled = chorusSideEnabledParam ? (static_cast<float>(*chorusSideEnabledParam) > 0.5f) : true;
            float sideGain = chorusSideGainParam ? static_cast<float>(*chorusSideGainParam) : 0.0f;
            
            // Get filter parameter values
            bool lpfEnabled = chorusLPFEnabledParam ? (static_cast<float>(*chorusLPFEnabledParam) > 0.5f) : false;
            float lpfCutoff = chorusLPFCutoffParam ? static_cast<float>(*chorusLPFCutoffParam) : 20000.0f;
            bool hpfEnabled = chorusHPFEnabledParam ? (static_cast<float>(*chorusHPFEnabledParam) > 0.5f) : false;
            float hpfCutoff = chorusHPFCutoffParam ? static_cast<float>(*chorusHPFCutoffParam) : 20.0f;
            
            // Update all chorus parameters in one call
            chorus.updateParameters(rate, depth, mix, baseDelay, voiceCount,
                                  stereoMode, stereoSpread,
                                  midEnabled, sideEnabled, sideGain,
                                  lpfEnabled, lpfCutoff, hpfEnabled, hpfCutoff);
            
            // Process audio through the multi-voice chorus effect
            chorus.processBlock(buffer);
        } else {
            // If chorus is disabled, just pass through the input
            // (or apply dry/wet mix if needed)
        }
    }

    bool AudioPluginAudioProcessor::hasEditor() const {
        return true; // (change this to false if you choose to not supply an editor)
    }

    juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
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