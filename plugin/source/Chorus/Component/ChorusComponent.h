#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Chorus.h"
#include "../../LFO/Component/LFOComponent.h"

namespace audio_plugin {

class ChorusComponent : public juce::Component,
                        private juce::Slider::Listener,
                        private juce::Button::Listener,
                        private juce::ComboBox::Listener
{
public:
    explicit ChorusComponent(Chorus& chorusRef, const juce::String& componentName = "Chorus");
    ~ChorusComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void updateFromChorus();
    void refreshLFOUIs();

private:
    Chorus& chorus;
    
    // Global Chorus Controls
    juce::Slider mixSlider;
    juce::Label mixLabel;
    
    juce::Slider baseDelaySlider;
    juce::Label baseDelayLabel;
    
    juce::ToggleButton enabledButton;
    
    juce::ComboBox stereoModeCombo;
    juce::Label stereoModeLabel;
    
    juce::Slider stereoSpreadSlider;
    juce::Label stereoSpreadLabel;
    
    juce::ToggleButton midEnabledButton;
    juce::ToggleButton sideEnabledButton;
    
    juce::Slider sideGainSlider;
    juce::Label sideGainLabel;
    
    // Array of LFO UIs - one for each LFO in the chorus
    juce::OwnedArray<LFOComponent> lfoComponents;
    
    // Scrollable container for LFO components
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;
    
    // Helper methods
    void createLFOUIs();
    void configureChorusLFOs();
    void setupGlobalControls();
    void updateGlobalControlsFromChorus();
    void layoutGlobalControls(juce::Rectangle<int> bounds);
    void updateLFOEnableState();
    
    // Listener implementations
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;
    
    // Layout constants
    static constexpr int GLOBAL_SECTION_HEIGHT = 250;
    static constexpr int ROW_HEIGHT = 30;
    static constexpr int LABEL_WIDTH = 120;
    static constexpr int CONTROL_WIDTH = 200;
    static constexpr int MARGIN = 10;
    static constexpr int SPACING = 5;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusComponent)
};

} // namespace audio_plugin
