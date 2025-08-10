#include "PluginProcessor.h"
#include "PluginEditor.h"

// Ensure M_PI is defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Static StringArray to avoid construction issues
static juce::StringArray createRhythmOptions()
{
    juce::StringArray options;
    options.add("1/2 Note");
    options.add("1/4 Note");  
    options.add("1/4 Triplet");
    options.add("1/8 Note");
    options.add("1/8 Triplet");
    options.add("1/16 Note");
    return options;
}

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
            // Essential LFO parameters for testing
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
                1.0f,                      // default value
                juce::AudioParameterFloatAttributes()
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
                0.8f,                      // default value
                juce::AudioParameterFloatAttributes()
            ),
            
            std::make_unique<juce::AudioParameterBool>(
                "lfo_invert",              // parameterID
                "LFO Invert",              // parameter name
                false,                     // default value (not inverted)
                juce::AudioParameterBoolAttributes()
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "lfo_phase_offset",        // parameterID
                "LFO Phase Offset",        // parameter name
                juce::NormalisableRange<float>(-180.0f, 180.0f, 1.0f),
                0.0f,                      // default value (0 degrees, center position)
                juce::AudioParameterFloatAttributes()
                    .withLabel("Deg.")        // degrees symbol
            ),
            
            std::make_unique<juce::AudioParameterFloat>(
                "lfo_symmetry",            // parameterID
                "LFO Symmetry",            // parameter name
                juce::NormalisableRange<float>(10.0f, 90.0f, 1.0f),
                50.0f,                     // default value (50%, symmetric)
                juce::AudioParameterFloatAttributes()
                    .withLabel("%")        // percentage symbol
            ),
            
            std::make_unique<juce::AudioParameterBool>(
                "lfo_sync_to_host",        // parameterID
                "LFO Sync to Host",        // parameter name
                false,                     // default value (manual frequency mode)
                juce::AudioParameterBoolAttributes()
            ),
            
            std::make_unique<juce::AudioParameterChoice>(
                "lfo_sync_rate",           // parameterID
                "LFO Rhythm",              // parameter name
                createRhythmOptions(),
                1,                         // default value (1/4 note)
                juce::AudioParameterChoiceAttributes()
            ),
            
            std::make_unique<juce::AudioParameterChoice>(
                "lfo_waveshape",           // parameterID
                "LFO Waveshape",           // parameter name
                juce::StringArray{"Sine", "Ramp Down", "Ramp Up", "Square", "Triangle", "Hump Down", "Hump Up"},
                0,                         // default value (Sine)
                juce::AudioParameterChoiceAttributes()
            )
        })
    {
        // Initialize parameter pointers for quick access
        lfoFrequencyParam = parameters.getRawParameterValue("lfo_frequency");
        lfoDepthParam = parameters.getRawParameterValue("lfo_depth");
        lfoEnabledParam = parameters.getRawParameterValue("lfo_enabled");
        lfoOutputLevelParam = parameters.getRawParameterValue("lfo_output_level");
        lfoInvertParam = parameters.getRawParameterValue("lfo_invert");
        lfoPhaseOffsetParam = parameters.getRawParameterValue("lfo_phase_offset");
        lfoSymmetryParam = parameters.getRawParameterValue("lfo_symmetry");
        lfoSyncToHostParam = parameters.getRawParameterValue("lfo_sync_to_host");
        lfoSyncRateParam = parameters.getRawParameterValue("lfo_sync_rate");
        lfoWaveshapeParam = parameters.getRawParameterValue("lfo_waveshape");
        
        DBG("PluginProcessor: Essential LFO parameters initialized");
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
        lfo.setInvert(lfoInvertParam->load() > 0.5f);
        lfo.setSymmetry(lfoSymmetryParam->load());
        lfo.setSyncToHost(lfoSyncToHostParam->load() > 0.5f);
        lfo.setSyncRate(static_cast<int>(lfoSyncRateParam->load()));
        
        // Set initial waveshape
        int waveshapeIndex = static_cast<int>(lfoWaveshapeParam->load());
        lfo.setWaveShape(static_cast<audio_plugin::LFO::WaveformType>(waveshapeIndex));
        
        // Convert degrees to radians for phase offset
        float phaseOffsetDegrees = lfoPhaseOffsetParam->load();
        float phaseOffsetRadians = phaseOffsetDegrees * (M_PI / 180.0f);
        lfo.setPhaseOffset(phaseOffsetRadians);
        
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
        bool newInvert = lfoInvertParam->load() > 0.5f;
        float newPhaseOffsetDegrees = lfoPhaseOffsetParam->load();
        float newSymmetry = lfoSymmetryParam->load();
        bool newSyncToHost = lfoSyncToHostParam->load() > 0.5f;
        float rawSyncRate = lfoSyncRateParam->load();
        int newSyncRate = static_cast<int>(rawSyncRate);
        float rawWaveshape = lfoWaveshapeParam->load();
        int newWaveshape = static_cast<int>(rawWaveshape);
        
        // Debug parameter values every 1000 samples to avoid spam, but always show sync rate changes
        static int debugCounter = 0;
        static float lastRawSyncRate = -999.0f;
        
        if (++debugCounter % 1000 == 0 || rawSyncRate != lastRawSyncRate) {
            DBG("Parameters: Freq=" << newFrequency << "Hz, Depth=" << newDepth 
                << ", SyncToHost=" << (newSyncToHost ? "true" : "false") << ", RawSyncRate=" << rawSyncRate 
                << ", IntSyncRate=" << newSyncRate);
            lastRawSyncRate = rawSyncRate;
        }
        
        // Only update if values have changed
        static float lastFrequency = -1.0f;
        static float lastDepth = -1.0f;
        static bool lastInvert = false;
        static float lastPhaseOffsetDegrees = -999.0f;
        static float lastSymmetry = -1.0f;
        static bool lastSyncToHost = false;
        static int lastSyncRate = -1;
        static int lastWaveshape = -1;
        static bool firstRun = true;
        
        if (newFrequency != lastFrequency) {
            lfo.setFrequency(newFrequency);
            lastFrequency = newFrequency;
        }
        
        if (newDepth != lastDepth) {
            lfo.setDepth(newDepth);
            lastDepth = newDepth;
        }
        
        if (newInvert != lastInvert) {
            lfo.setInvert(newInvert);
            lastInvert = newInvert;
        }
        
        if (newPhaseOffsetDegrees != lastPhaseOffsetDegrees) {
            // Convert degrees to radians
            float phaseOffsetRadians = newPhaseOffsetDegrees * (M_PI / 180.0f);
            lfo.setPhaseOffset(phaseOffsetRadians);
            lastPhaseOffsetDegrees = newPhaseOffsetDegrees;
        }
        
        if (newSymmetry != lastSymmetry) {
            lfo.setSymmetry(newSymmetry);
            lastSymmetry = newSymmetry;
        }
        
        if (newSyncToHost != lastSyncToHost || firstRun) {
            lfo.setSyncToHost(newSyncToHost);
            lastSyncToHost = newSyncToHost;
        }

        // Always log sync rate for debugging
        DBG("PluginProcessor: Checking sync rate - Current=" << newSyncRate << ", Last=" << lastSyncRate << ", FirstRun=" << (firstRun ? "true" : "false"));
        
        if (newSyncRate != lastSyncRate || firstRun) {
            DBG("PluginProcessor: Sync rate parameter changed from " << lastSyncRate << " to " << newSyncRate);
            lfo.setSyncRate(newSyncRate);
            lastSyncRate = newSyncRate;
        } else {
            DBG("PluginProcessor: Sync rate unchanged, not updating LFO");
        }
        
        if (newWaveshape != lastWaveshape || firstRun) {
            DBG("PluginProcessor: Waveshape parameter changed from " << lastWaveshape << " to " << newWaveshape);
            lfo.setWaveShape(static_cast<audio_plugin::LFO::WaveformType>(newWaveshape));
            lastWaveshape = newWaveshape;
        }
        
        firstRun = false;
        
        // Get host timing information
        juce::AudioPlayHead* playHead = getPlayHead();
        if (playHead != nullptr) {
            juce::AudioPlayHead::CurrentPositionInfo positionInfo;
            if (playHead->getCurrentPosition(positionInfo)) {
                double hostBPM = positionInfo.bpm > 0.0 ? positionInfo.bpm : 120.0;
                bool isPlaying = positionInfo.isPlaying;
                lfo.updateHostInfo(hostBPM, isPlaying);
            }
        } else {
            // Fallback when no host available
            lfo.updateHostInfo(120.0, true);
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