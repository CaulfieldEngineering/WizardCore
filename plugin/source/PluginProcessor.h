#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
//#include "DelayLine/DelayLine.h"
#include "Chorus/Chorus.h"
#include "LFO/LFO.h"

namespace audio_plugin {

class AudioPluginAudioProcessor : public juce::AudioProcessor,
                                  public juce::ValueTree::Listener
{
public:
    //==============================================================================
    static constexpr int MAX_CHORUS_VOICES = 5;  // Maximum number of chorus voices
    
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter change handling
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                 const juce::Identifier& property) override;

    //==============================================================================
    // Public access to parameters for the editor
    juce::AudioProcessorValueTreeState parameters;
    
    // Public access to DSP objects for the editor (for debug UI)
    WizardCore::Chorus& getChorus() { return chorus; }
    WizardCore::LFO& getTestLFO() { return testLFO; }

private:
    //==============================================================================
	
	// DelayLine ======================================================
		//// Delay line instance
		//DelayLine delayLine;
		//
		//// Parameter pointers for quick access
		//std::atomic<float>* delayTimeParam = nullptr;
		//std::atomic<float>* mixParam = nullptr;
		//
		//// Smoothed value for mix parameter only (delay smoothing is in DelayLine)
		//juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;

	// Chorus ============================================================
	WizardCore::Chorus chorus;  // Initialized with MAX_CHORUS_VOICES voices in constructor
	
	// Test LFO for UI development
	WizardCore::LFO testLFO;  // Standalone LFO for testing the UI
	
	// Global Chorus parameter pointers for quick access
	std::atomic<float>* chorusRateParam = nullptr;
	std::atomic<float>* chorusDepthParam = nullptr;
	std::atomic<float>* chorusMixParam = nullptr;
	std::atomic<float>* chorusBaseDelayParam = nullptr;
	std::atomic<float>* chorusVoiceCountParam = nullptr;
	std::atomic<float>* chorusEnabledParam = nullptr;
	std::atomic<float>* chorusDelayTypeParam = nullptr;
	
	// Stereo Chorus parameters
	std::atomic<float>* chorusStereoModeParam = nullptr;
	std::atomic<float>* chorusStereoSpreadParam = nullptr;
	
	// Mid-Side Chorus parameters
	std::atomic<float>* chorusMidEnabledParam = nullptr;
	std::atomic<float>* chorusSideEnabledParam = nullptr;
	std::atomic<float>* chorusSideGainParam = nullptr;
	
	// LPF parameters
	std::atomic<float>* chorusLPFEnabledParam = nullptr;
	std::atomic<float>* chorusLPFCutoffParam = nullptr;
	
	// HPF parameters
	std::atomic<float>* chorusHPFEnabledParam = nullptr;
	std::atomic<float>* chorusHPFCutoffParam = nullptr;
	
	// Simple flag to track delay type changes
	std::atomic<bool> delayTypeChanged{false};
	
	// Parameter change flags for all chorus parameters
	std::atomic<bool> chorusRateChanged{false};
	std::atomic<bool> chorusDepthChanged{false};
	std::atomic<bool> chorusMixChanged{false};
	std::atomic<bool> chorusBaseDelayChanged{false};
	std::atomic<bool> chorusVoiceCountChanged{false};
	std::atomic<bool> chorusEnabledChanged{false};
	
	// Stereo Chorus parameter change flags
	std::atomic<bool> chorusStereoModeChanged{false};
	std::atomic<bool> chorusStereoSpreadChanged{false};
	
	// Mid-Side Chorus parameter change flags
	std::atomic<bool> chorusMidEnabledChanged{false};
	std::atomic<bool> chorusSideEnabledChanged{false};
	std::atomic<bool> chorusSideGainChanged{false};
	
	// LPF parameter change flags
	std::atomic<bool> chorusLPFEnabledChanged{false};
	std::atomic<bool> chorusLPFCutoffChanged{false};
	
	// HPF parameter change flags
	std::atomic<bool> chorusHPFEnabledChanged{false};
	std::atomic<bool> chorusHPFCutoffChanged{false};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

} // namespace audio_plugin