#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "Chorus/Component/ChorusComponent.h"

namespace audio_plugin {

    class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor {
    public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);
    ~AudioPluginAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

    private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor &processorRef;

    juce::Slider delayTimeSlider;
    
    // Main UI Component for Chorus (includes embedded LFO UIs)
    std::unique_ptr<ChorusComponent> chorusUIComponent;

    void sliderValueChanged(juce::Slider* slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
    };
    
} // namespace audio_plugin