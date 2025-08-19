#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../LFO.h"

namespace audio_plugin {

/**
 * @brief Simplified UI Component for LFO control
 * 
 * Provides linear sliders and controls for all LFO parameters.
 * Clean, simple interface without visual displays or real-time updates.
 */
class LFOComponent : public juce::Component,
                     private juce::Slider::Listener,
                     private juce::Button::Listener,
                     private juce::ComboBox::Listener
{
public:
    /**
     * @brief Constructor
     * @param lfoRef Reference to the LFO object to control
     * @param componentName Display name for this component
     */
    explicit LFOComponent(LFO& lfoRef, const juce::String& componentName = "LFO");
    
    /**
     * @brief Destructor
     */
    ~LFOComponent() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Manual parameter updates (call when parameters change externally)
    void updateFromLFO();
    
    // Configuration methods for customizing UI limits (for specific applications like chorus)
    void setFrequencyRange(double minFreq, double maxFreq, double interval);
    void setFrequencySkewFactor(double skewFactor);
    void setSyncToHostEnabled(bool enabled);

private:
    // Reference to the LFO we're controlling
    LFO& lfo;
    
    // === ALL LFO PARAMETERS ===
    
    // Primary controls
    juce::Slider frequencySlider;
    juce::Label frequencyLabel;
    
    juce::Slider depthSlider;
    juce::Label depthLabel;
    
    juce::ToggleButton enabledButton;
    
    // Phase and symmetry
    juce::Slider phaseOffsetSlider;
    juce::Label phaseOffsetLabel;
    
    juce::Slider symmetrySlider;
    juce::Label symmetryLabel;
    
    // Waveform controls
    juce::ToggleButton invertButton;
    
    juce::ComboBox waveformCombo;
    juce::Label waveformLabel;
    
    // Host sync controls
    juce::ToggleButton syncToHostButton;
    
    juce::ComboBox syncRateCombo;
    juce::Label syncRateLabel;
    
    // === LISTENER IMPLEMENTATIONS ===
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;
    
    // === HELPER METHODS ===
    void setupControls();
    void updateControlsFromLFO();
    void layoutControls();
    
    // Layout constants
    static constexpr int ROW_HEIGHT = 30;
    static constexpr int LABEL_WIDTH = 100;
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int MARGIN = 10;
    static constexpr int SPACING = 5;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOComponent)
};

} // namespace audio_plugin
