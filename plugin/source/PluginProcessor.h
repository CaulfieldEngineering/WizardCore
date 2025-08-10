#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
//#include "DelayLine/DelayLine.h"
#include "LFO/LFO.h"

namespace audio_plugin {

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
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
    // Public access to parameters for the editor
    juce::AudioProcessorValueTreeState parameters;

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

	// LFO ============================================================
	LFO lfo;
	
	// LFO parameter pointers for quick access
	std::atomic<float>* lfoFrequencyParam = nullptr;
	std::atomic<float>* lfoDepthParam = nullptr;
	std::atomic<float>* lfoEnabledParam = nullptr;
	std::atomic<float>* lfoOutputLevelParam = nullptr;
	std::atomic<float>* lfoInvertParam = nullptr;
	std::atomic<float>* lfoPhaseOffsetParam = nullptr;
	std::atomic<float>* lfoSymmetryParam = nullptr;
	std::atomic<float>* lfoSyncToHostParam = nullptr;
	std::atomic<float>* lfoSyncRateParam = nullptr;
	std::atomic<float>* lfoWaveshapeParam = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

} // namespace audio_plugin