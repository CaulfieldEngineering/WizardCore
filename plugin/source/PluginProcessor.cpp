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
            
            std::make_unique<juce::AudioParameterChoice>(
                "chorus_delay_type",        // parameterID
                "Delay Type",               // parameter name
                juce::StringArray{"Digital", "Bucket Brigade"}, // choices
                1                           // default value (Bucket Brigade)
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
        // Add this processor as a listener to parameter changes
        parameters.state.addListener(this);
        
        // Initialize global parameter pointers for quick access
        chorusRateParam = parameters.getRawParameterValue("chorus_rate");
        chorusDepthParam = parameters.getRawParameterValue("chorus_depth");
        chorusMixParam = parameters.getRawParameterValue("chorus_mix");
        chorusBaseDelayParam = parameters.getRawParameterValue("chorus_base_delay");
        chorusVoiceCountParam = parameters.getRawParameterValue("chorus_voice_count");
        chorusEnabledParam = parameters.getRawParameterValue("chorus_enabled");
        chorusDelayTypeParam = parameters.getRawParameterValue("chorus_delay_type");
        
        // Debug output for delay type parameter initialization
        if (chorusDelayTypeParam) {
            float initialDelayTypeChoice = *chorusDelayTypeParam;
            DBG("PluginProcessor: Initial delay type parameter value: " << initialDelayTypeChoice 
                << " (0.0=Digital, 1.0=Bucket Brigade)");
        }
        
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
        if (!chorusRateParam || !chorusDepthParam || !chorusMixParam || !chorusBaseDelayParam || !chorusVoiceCountParam || !chorusEnabledParam || !chorusDelayTypeParam) {
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

    AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
        // Remove this processor as a listener to parameter changes
        parameters.state.removeListener(this);
    }

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
        chorus.setVoiceCount(1);  // Start with 1 voice
        chorus.setMix(0.5f);     // 50% wet/dry mix
        chorus.setBaseDelay(30.0f); // 30ms base delay
        
        // Set initial enabled state from parameter
        const bool initiallyEnabled = chorusEnabledParam ? (*chorusEnabledParam > 0.5f) : true;
        chorus.setEnabled(initiallyEnabled);
        
        // Set all parameter change flags to ensure initial update
        delayTypeChanged.store(true);
        chorusRateChanged.store(true);
        chorusDepthChanged.store(true);
        chorusMixChanged.store(true);
        chorusBaseDelayChanged.store(true);
        chorusVoiceCountChanged.store(true);
        chorusEnabledChanged.store(true);

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

        // Build and apply parameter updates only when something changed
        const bool hasAnyParamChange = delayTypeChanged.load() ||
                                       chorusRateChanged.load() ||
                                       chorusDepthChanged.load() ||
                                       chorusMixChanged.load() ||
                                       chorusBaseDelayChanged.load() ||
                                       chorusVoiceCountChanged.load() ||
                                       chorusStereoModeChanged.load() ||
                                       chorusStereoSpreadChanged.load() ||
                                       chorusMidEnabledChanged.load() ||
                                       chorusSideEnabledChanged.load() ||
                                       chorusSideGainChanged.load() ||
                                       chorusLPFEnabledChanged.load() ||
                                       chorusLPFCutoffChanged.load() ||
                                       chorusHPFEnabledChanged.load() ||
                                       chorusHPFCutoffChanged.load() ||
                                       chorusEnabledChanged.load();

        if (hasAnyParamChange) {
            Chorus::UpdateArgs args;

            if (delayTypeChanged.load()) {
                if (chorusDelayTypeParam) {
                    const float delayTypeChoice = *chorusDelayTypeParam;
                    const DelayType delayType = (delayTypeChoice < 0.5f) ? DelayType::DigitalDelay : DelayType::BBDelay;
                    args.delayType = delayType;
                    DBG("PluginProcessor: Delay type updated to: " << (delayType == DelayType::DigitalDelay ? "Digital" : "Bucket Brigade")
                        << " (raw value: " << delayTypeChoice << ")");
                }
            }

            if (chorusRateChanged.load() && chorusRateParam)       args.rate = static_cast<float>(*chorusRateParam);
            if (chorusDepthChanged.load() && chorusDepthParam)     args.depth = static_cast<float>(*chorusDepthParam);
            if (chorusMixChanged.load() && chorusMixParam)         args.mix = static_cast<float>(*chorusMixParam);
            if (chorusBaseDelayChanged.load() && chorusBaseDelayParam) args.baseDelayMs = static_cast<float>(*chorusBaseDelayParam);
            if (chorusVoiceCountChanged.load() && chorusVoiceCountParam) args.voiceCount = static_cast<int>(*chorusVoiceCountParam);

            if (chorusStereoModeChanged.load() && chorusStereoModeParam)   args.stereoMode = static_cast<int>(*chorusStereoModeParam);
            if (chorusStereoSpreadChanged.load() && chorusStereoSpreadParam) args.stereoSpread = static_cast<float>(*chorusStereoSpreadParam);

            if (chorusMidEnabledChanged.load() && chorusMidEnabledParam)   args.midEnabled = (static_cast<float>(*chorusMidEnabledParam) > 0.5f);
            if (chorusSideEnabledChanged.load() && chorusSideEnabledParam) args.sideEnabled = (static_cast<float>(*chorusSideEnabledParam) > 0.5f);
            if (chorusSideGainChanged.load() && chorusSideGainParam)       args.sideGainDb = static_cast<float>(*chorusSideGainParam);

            if (chorusLPFEnabledChanged.load() && chorusLPFEnabledParam)   args.lpfEnabled = (static_cast<float>(*chorusLPFEnabledParam) > 0.5f);
            if (chorusLPFCutoffChanged.load() && chorusLPFCutoffParam)     args.lpfCutoffHz = static_cast<float>(*chorusLPFCutoffParam);
            if (chorusHPFEnabledChanged.load() && chorusHPFEnabledParam)   args.hpfEnabled = (static_cast<float>(*chorusHPFEnabledParam) > 0.5f);
            if (chorusHPFCutoffChanged.load() && chorusHPFCutoffParam)     args.hpfCutoffHz = static_cast<float>(*chorusHPFCutoffParam);

            // Apply parameter updates in one call
            chorus.updateParameters(args);

            // Update enabled state if changed
            if (chorusEnabledChanged.load() && chorusEnabledParam) {
                const bool enabledNow = (*chorusEnabledParam > 0.5f);
                chorus.setEnabled(enabledNow);
            }

            // Clear all parameter change flags
            delayTypeChanged.store(false);
            chorusRateChanged.store(false);
            chorusDepthChanged.store(false);
            chorusMixChanged.store(false);
            chorusBaseDelayChanged.store(false);
            chorusVoiceCountChanged.store(false);
            chorusEnabledChanged.store(false);
            chorusStereoModeChanged.store(false);
            chorusStereoSpreadChanged.store(false);
            chorusMidEnabledChanged.store(false);
            chorusSideEnabledChanged.store(false);
            chorusSideGainChanged.store(false);
            chorusLPFEnabledChanged.store(false);
            chorusLPFCutoffChanged.store(false);
            chorusHPFEnabledChanged.store(false);
            chorusHPFCutoffChanged.store(false);

            DBG("PluginProcessor: Chorus parameters updated");
        }
        
        // Process audio through the multi-voice chorus effect if enabled
        const bool chorusIsEnabled = chorusEnabledParam ? (*chorusEnabledParam > 0.5f) : true;
        if (chorusIsEnabled) {
            chorus.processBlock(buffer);
        } else {
            // If chorus is disabled, just pass through the input
        }
    }
    
    void AudioPluginAudioProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                           const juce::Identifier& property)
    {
        // Handle parameter changes efficiently
        // This is called when any parameter value changes, not on every audio sample
        // This approach is much more efficient than checking parameter values in processBlock
        // because it only runs when parameters actually change, not on every audio sample
        
        // Debug output to show which property changed
        DBG("PluginProcessor: Property changed: " << property.toString() << " in tree: " << treeWhosePropertyHasChanged.getType().toString());
        
        // In JUCE's APVTS, parameter values are stored under a "value" property
        // We need to check the parent tree to identify which parameter this belongs to
        if (property == juce::Identifier("value")) {
            // Get the parent tree (which should be the parameter tree)
            auto parentTree = treeWhosePropertyHasChanged.getParent();
            if (parentTree.isValid()) {
                // The parent tree should be the individual parameter
                juce::String paramID = parentTree.getType().toString();
                DBG("PluginProcessor: Parameter ID: " << paramID);
                
                // Also check the grandparent to see the full tree structure
                auto grandParentTree = parentTree.getParent();
                if (grandParentTree.isValid()) {
                    DBG("PluginProcessor: Grandparent tree type: " << grandParentTree.getType().toString());
                }
                
                // Check if the tree has a name property (alternative way to identify parameters)
                if (parentTree.hasProperty("name")) {
                    juce::String paramName = parentTree.getProperty("name").toString();
                    DBG("PluginProcessor: Parameter name from property: " << paramName);
                    
                    // Handle delay type changes using the name property
                    if (paramName == "chorus_delay_type") {
                        delayTypeChanged.store(true);
                        DBG("PluginProcessor: Delay type change detected via name property - flag set");
                        return;
                    }
                }
                
                // Handle delay type changes using tree type
                if (paramID == "chorus_delay_type") {
                    // Just set a flag - the actual update will happen in processBlock
                    delayTypeChanged.store(true);
                    DBG("PluginProcessor: Delay type change detected via tree type - flag set");
                }
                
                // Handle all other chorus parameter changes
                else if (paramID == "chorus_rate") {
                    chorusRateChanged.store(true);
                    DBG("PluginProcessor: Chorus rate change detected - flag set");
                }
                else if (paramID == "chorus_depth") {
                    chorusDepthChanged.store(true);
                    DBG("PluginProcessor: Chorus depth change detected - flag set");
                }
                else if (paramID == "chorus_mix") {
                    chorusMixChanged.store(true);
                    DBG("PluginProcessor: Chorus mix change detected - flag set");
                }
                else if (paramID == "chorus_base_delay") {
                    chorusBaseDelayChanged.store(true);
                    DBG("PluginProcessor: Chorus base delay change detected - flag set");
                }
                else if (paramID == "chorus_voice_count") {
                    chorusVoiceCountChanged.store(true);
                    DBG("PluginProcessor: Chorus voice count change detected - flag set");
                }
                else if (paramID == "chorus_enabled") {
                    chorusEnabledChanged.store(true);
                    DBG("PluginProcessor: Chorus enabled change detected - flag set");
                }
                else if (paramID == "chorus_stereo_mode") {
                    chorusStereoModeChanged.store(true);
                    DBG("PluginProcessor: Chorus stereo mode change detected - flag set");
                }
                else if (paramID == "chorus_stereo_spread") {
                    chorusStereoSpreadChanged.store(true);
                    DBG("PluginProcessor: Chorus stereo spread change detected - flag set");
                }
                else if (paramID == "chorus_mid_enabled") {
                    chorusMidEnabledChanged.store(true);
                    DBG("PluginProcessor: Chorus mid enabled change detected - flag set");
                }
                else if (paramID == "chorus_side_enabled") {
                    chorusSideEnabledChanged.store(true);
                    DBG("PluginProcessor: Chorus side enabled change detected - flag set");
                }
                else if (paramID == "chorus_side_gain") {
                    chorusSideGainChanged.store(true);
                    DBG("PluginProcessor: Chorus side gain change detected - flag set");
                }
                else if (paramID == "chorus_lpf_enabled") {
                    chorusLPFEnabledChanged.store(true);
                    DBG("PluginProcessor: Chorus LPF enabled change detected - flag set");
                }
                else if (paramID == "chorus_lpf_cutoff") {
                    chorusLPFCutoffChanged.store(true);
                    DBG("PluginProcessor: Chorus LPF cutoff change detected - flag set");
                }
                else if (paramID == "chorus_hpf_enabled") {
                    chorusHPFEnabledChanged.store(true);
                    DBG("PluginProcessor: Chorus HPF enabled change detected - flag set");
                }
                else if (paramID == "chorus_hpf_cutoff") {
                    chorusHPFCutoffChanged.store(true);
                    DBG("PluginProcessor: Chorus HPF cutoff change detected - flag set");
                }
            }
        }
        
        // Fallback: If we can't identify which parameter changed, just check if it might be the delay type
        // This ensures we don't miss delay type changes even if the tree structure is unexpected
        if (chorusDelayTypeParam) {
            static float lastDelayTypeValue = -1.0f;
            float currentDelayTypeValue = *chorusDelayTypeParam;
            if (currentDelayTypeValue != lastDelayTypeValue) {
                lastDelayTypeValue = currentDelayTypeValue;
                delayTypeChanged.store(true);
                DBG("PluginProcessor: Delay type change detected via fallback - flag set");
            }
        }
        
        // Fallback parameter change detection for all other chorus parameters
        // This ensures we don't miss parameter changes even if the tree structure is unexpected
        
        // Rate parameter fallback
        if (chorusRateParam) {
            static float lastRateValue = -1.0f;
            float currentRateValue = *chorusRateParam;
            if (currentRateValue != lastRateValue) {
                lastRateValue = currentRateValue;
                chorusRateChanged.store(true);
                DBG("PluginProcessor: Chorus rate change detected via fallback - flag set");
            }
        }
        
        // Depth parameter fallback
        if (chorusDepthParam) {
            static float lastDepthValue = -1.0f;
            float currentDepthValue = *chorusDepthParam;
            if (currentDepthValue != lastDepthValue) {
                lastDepthValue = currentDepthValue;
                chorusDepthChanged.store(true);
                DBG("PluginProcessor: Chorus depth change detected via fallback - flag set");
            }
        }
        
        // Mix parameter fallback
        if (chorusMixParam) {
            static float lastMixValue = -1.0f;
            float currentMixValue = *chorusMixParam;
            if (currentMixValue != lastMixValue) {
                lastMixValue = currentMixValue;
                chorusMixChanged.store(true);
                DBG("PluginProcessor: Chorus mix change detected via fallback - flag set");
            }
        }
        
        // Base delay parameter fallback
        if (chorusBaseDelayParam) {
            static float lastBaseDelayValue = -1.0f;
            float currentBaseDelayValue = *chorusBaseDelayParam;
            if (currentBaseDelayValue != lastBaseDelayValue) {
                lastBaseDelayValue = currentBaseDelayValue;
                chorusBaseDelayChanged.store(true);
                DBG("PluginProcessor: Chorus base delay change detected via fallback - flag set");
            }
        }
        
        // Voice count parameter fallback
        if (chorusVoiceCountParam) {
            static float lastVoiceCountValue = -1.0f;
            float currentVoiceCountValue = *chorusVoiceCountParam;
            if (currentVoiceCountValue != lastVoiceCountValue) {
                lastVoiceCountValue = currentVoiceCountValue;
                chorusVoiceCountChanged.store(true);
                DBG("PluginProcessor: Chorus voice count change detected via fallback - flag set");
            }
        }
        
        // Enabled parameter fallback
        if (chorusEnabledParam) {
            static float lastEnabledValue = -1.0f;
            float currentEnabledValue = *chorusEnabledParam;
            if (currentEnabledValue != lastEnabledValue) {
                lastEnabledValue = currentEnabledValue;
                chorusEnabledChanged.store(true);
                DBG("PluginProcessor: Chorus enabled change detected via fallback - flag set");
            }
        }
        
        // Stereo mode parameter fallback
        if (chorusStereoModeParam) {
            static float lastStereoModeValue = -1.0f;
            float currentStereoModeValue = *chorusStereoModeParam;
            if (currentStereoModeValue != lastStereoModeValue) {
                lastStereoModeValue = currentStereoModeValue;
                chorusStereoModeChanged.store(true);
                DBG("PluginProcessor: Chorus stereo mode change detected via fallback - flag set");
            }
        }
        
        // Stereo spread parameter fallback
        if (chorusStereoSpreadParam) {
            static float lastStereoSpreadValue = -1.0f;
            float currentStereoSpreadValue = *chorusStereoSpreadParam;
            if (currentStereoSpreadValue != lastStereoSpreadValue) {
                lastStereoSpreadValue = currentStereoSpreadValue;
                chorusStereoSpreadChanged.store(true);
                DBG("PluginProcessor: Chorus stereo spread change detected via fallback - flag set");
            }
        }
        
        // Mid enabled parameter fallback
        if (chorusMidEnabledParam) {
            static float lastMidEnabledValue = -1.0f;
            float currentMidEnabledValue = *chorusMidEnabledParam;
            if (currentMidEnabledValue != lastMidEnabledValue) {
                lastMidEnabledValue = currentMidEnabledValue;
                chorusMidEnabledChanged.store(true);
                DBG("PluginProcessor: Chorus mid enabled change detected via fallback - flag set");
            }
        }
        
        // Side enabled parameter fallback
        if (chorusSideEnabledParam) {
            static float lastSideEnabledValue = -1.0f;
            float currentSideEnabledValue = *chorusSideEnabledParam;
            if (currentSideEnabledValue != lastSideEnabledValue) {
                lastSideEnabledValue = currentSideEnabledValue;
                chorusSideEnabledChanged.store(true);
                DBG("PluginProcessor: Chorus side enabled change detected via fallback - flag set");
            }
        }
        
        // Side gain parameter fallback
        if (chorusSideGainParam) {
            static float lastSideGainValue = -1.0f;
            float currentSideGainValue = *chorusSideGainParam;
            if (currentSideGainValue != lastSideGainValue) {
                lastSideGainValue = currentSideGainValue;
                chorusSideGainChanged.store(true);
                DBG("PluginProcessor: Chorus side gain change detected via fallback - flag set");
            }
        }
        
        // LPF enabled parameter fallback
        if (chorusLPFEnabledParam) {
            static float lastLPFEnabledValue = -1.0f;
            float currentLPFEnabledValue = *chorusLPFEnabledParam;
            if (currentLPFEnabledValue != lastLPFEnabledValue) {
                lastLPFEnabledValue = currentLPFEnabledValue;
                chorusLPFEnabledChanged.store(true);
                DBG("PluginProcessor: Chorus LPF enabled change detected via fallback - flag set");
            }
        }
        
        // LPF cutoff parameter fallback
        if (chorusLPFCutoffParam) {
            static float lastLPFCutoffValue = -1.0f;
            float currentLPFCutoffValue = *chorusLPFCutoffParam;
            if (currentLPFCutoffValue != lastLPFCutoffValue) {
                lastLPFCutoffValue = currentLPFCutoffValue;
                chorusLPFCutoffChanged.store(true);
                DBG("PluginProcessor: Chorus LPF cutoff change detected via fallback - flag set");
            }
        }
        
        // HPF enabled parameter fallback
        if (chorusHPFEnabledParam) {
            static float lastHPFEnabledValue = -1.0f;
            float currentHPFEnabledValue = *chorusHPFEnabledParam;
            if (currentHPFEnabledValue != lastHPFEnabledValue) {
                lastHPFEnabledValue = currentHPFEnabledValue;
                chorusHPFEnabledChanged.store(true);
                DBG("PluginProcessor: Chorus HPF enabled change detected via fallback - flag set");
            }
        }
        
        // HPF cutoff parameter fallback
        if (chorusHPFCutoffParam) {
            static float lastHPFCutoffValue = -1.0f;
            float currentHPFCutoffValue = *chorusHPFCutoffParam;
            if (currentHPFCutoffValue != lastHPFCutoffValue) {
                lastHPFCutoffValue = currentHPFCutoffValue;
                chorusHPFCutoffChanged.store(true);
                DBG("PluginProcessor: Chorus HPF cutoff change detected via fallback - flag set");
            }
        }
        
        // Note: Other parameters (rate, depth, mix, etc.) are still handled in processBlock
        // because they need to be applied on every audio sample for smooth interpolation
        // Only parameters that require expensive operations (like recreating delay lines)
        // should be handled here in valueTreePropertyChanged
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