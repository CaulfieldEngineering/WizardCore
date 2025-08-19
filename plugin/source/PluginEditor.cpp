#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace audio_plugin {
    AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
        AudioPluginAudioProcessor &p)
        : AudioProcessorEditor(&p), processorRef(p) {
    juce::ignoreUnused(processorRef);
    
    // Create the main Chorus UI Component
    chorusUIComponent = std::make_unique<ChorusComponent>(processorRef.getChorus(), "Main Chorus");
    addAndMakeVisible(chorusUIComponent.get());
    
    // Initialize UI with current LFO values
    chorusUIComponent->updateFromChorus();
    
    // Set initial size - will be adjusted based to content
    setSize(800, 600);
    
    // Make the window resizable
    setResizable(true, true);
    setResizeLimits(600, 400, 1200, 800);
    }

    AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
        // Clean up the Chorus UI Component (DSP objects remain in processor)
        chorusUIComponent.reset();
    }

    void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
    // (Our component is opaque, so we must completely fill the background with a
    // solid colour)
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);

    #ifdef DEMO_VERSION
        g.drawFittedText("Hello World! - Demo Version!", getLocalBounds(),
                        juce::Justification::centred, 1);
    #else
       g.drawFittedText("Hello World!", getLocalBounds(),
                        juce::Justification::centred, 1);
    #endif


    }

    void AudioPluginAudioProcessorEditor::resized() {
        auto bounds = getLocalBounds();
        
        // Give the entire space to the Chorus UI Component
        if (chorusUIComponent) {
            chorusUIComponent->setBounds(bounds);
        }
        
        // This is generally where you'll want to lay out the positions of any
        // other subcomponents in your editor..
    }
    
    void AudioPluginAudioProcessorEditor::sliderValueChanged(juce::Slider* slider) {
        // Handle slider changes here
        juce::ignoreUnused(slider);
    }
} // namespace audio_plugin