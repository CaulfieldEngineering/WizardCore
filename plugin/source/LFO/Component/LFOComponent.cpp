#include "LFOComponent.h"

namespace audio_plugin {

LFOComponent::LFOComponent(LFO& lfoRef, const juce::String& componentName)
    : lfo(lfoRef)
{
    // Set the component name (defaults to "LFO" if empty)
    setName(componentName.isEmpty() ? "LFO" : componentName);
    
    setupControls();
    updateControlsFromLFO();
}

LFOComponent::~LFOComponent()
{
}

void LFOComponent::setupControls()
{
    // === FREQUENCY ===
    frequencySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    frequencySlider.setRange(0.001, 1000.0, 0.001);
    frequencySlider.setSkewFactorFromMidPoint(10.0); // Make lower frequencies easier to control
    frequencySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.addListener(this);
    addAndMakeVisible(frequencySlider);
    
    frequencyLabel.setText("Frequency", juce::dontSendNotification);
    frequencyLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(frequencyLabel);
    
    // === DEPTH ===
    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setRange(0.0, 1.0, 0.01);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    depthSlider.addListener(this);
    addAndMakeVisible(depthSlider);
    
    depthLabel.setText("Depth", juce::dontSendNotification);
    depthLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(depthLabel);
    
    // === ENABLED ===
    enabledButton.setButtonText("Enabled");
    enabledButton.addListener(this);
    addAndMakeVisible(enabledButton);
    
    // === PHASE OFFSET ===
    phaseOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    phaseOffsetSlider.setRange(-180.0, 180.0, 1.0);
    phaseOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    phaseOffsetSlider.setTextValueSuffix(" deg");
    phaseOffsetSlider.addListener(this);
    addAndMakeVisible(phaseOffsetSlider);
    
    phaseOffsetLabel.setText("Phase Offset", juce::dontSendNotification);
    phaseOffsetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(phaseOffsetLabel);
    
    // === SYMMETRY ===
    symmetrySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    symmetrySlider.setRange(10.0, 90.0, 1.0);
    symmetrySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    symmetrySlider.setTextValueSuffix("%");
    symmetrySlider.addListener(this);
    addAndMakeVisible(symmetrySlider);
    
    symmetryLabel.setText("Symmetry", juce::dontSendNotification);
    symmetryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(symmetryLabel);
    
    // === INVERT ===
    invertButton.setButtonText("Invert");
    invertButton.addListener(this);
    addAndMakeVisible(invertButton);
    
    // === WAVEFORM ===
    waveformCombo.addItem("Sine", 1);
    waveformCombo.addItem("Ramp Down", 2);
    waveformCombo.addItem("Ramp Up", 3);
    waveformCombo.addItem("Square", 4);
    waveformCombo.addItem("Triangle", 5);
    waveformCombo.addItem("Hump Down", 6);
    waveformCombo.addItem("Hump Up", 7);
    waveformCombo.addListener(this);
    addAndMakeVisible(waveformCombo);
    
    waveformLabel.setText("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(waveformLabel);
    
    // === SYNC TO HOST ===
    syncToHostButton.setButtonText("Sync to Host");
    syncToHostButton.addListener(this);
    addAndMakeVisible(syncToHostButton);
    
    // === SYNC RATE ===
    syncRateCombo.addItem("1/2 Note", 1);
    syncRateCombo.addItem("1/4 Note", 2);
    syncRateCombo.addItem("1/4 Triplet", 3);
    syncRateCombo.addItem("1/8 Note", 4);
    syncRateCombo.addItem("1/8 Triplet", 5);
    syncRateCombo.addItem("1/16 Note", 6);
    syncRateCombo.addListener(this);
    addAndMakeVisible(syncRateCombo);
    
    syncRateLabel.setText("Sync Rate", juce::dontSendNotification);
    syncRateLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(syncRateLabel);
}

void LFOComponent::updateControlsFromLFO()
{
    // Update all controls to match current LFO state
    frequencySlider.setValue(lfo.getFrequency(), juce::dontSendNotification);
    depthSlider.setValue(lfo.getDepth(), juce::dontSendNotification);
    enabledButton.setToggleState(lfo.isEnabled(), juce::dontSendNotification);
    
    // Convert radians to degrees for display
    phaseOffsetSlider.setValue(lfo.getPhaseOffset() * 180.0 / juce::MathConstants<double>::pi, 
                              juce::dontSendNotification);
    
    // Convert 0-1 symmetry to percentage
    symmetrySlider.setValue(lfo.getSymmetry() * 100.0, juce::dontSendNotification);
    
    invertButton.setToggleState(lfo.getInvert(), juce::dontSendNotification);
    
    // Set combo box selections
    waveformCombo.setSelectedId(static_cast<int>(lfo.getWaveformType()) + 1, juce::dontSendNotification);
    syncToHostButton.setToggleState(lfo.getSyncToHost(), juce::dontSendNotification);
    syncRateCombo.setSelectedId(lfo.getSyncRate() + 1, juce::dontSendNotification);
}

void LFOComponent::paint(juce::Graphics& g)
{
    // Simple background
    g.fillAll(juce::Colour::fromRGB(50, 50, 55));
    
    // Border
    g.setColour(juce::Colour::fromRGB(100, 100, 105));
    g.drawRect(getLocalBounds(), 1);
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText(getName(), 10, 5, getWidth() - 20, 25, juce::Justification::centred);
}

void LFOComponent::resized()
{
    layoutControls();
}

void LFOComponent::layoutControls()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(25); // Title space
    bounds.reduce(MARGIN, MARGIN);
    
    int currentY = bounds.getY();
    
    // Row 1: Frequency
    auto row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    frequencyLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    frequencySlider.setBounds(row);
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 2: Depth
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    depthLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    depthSlider.setBounds(row);
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 3: Phase Offset
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    phaseOffsetLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    phaseOffsetSlider.setBounds(row);
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 4: Symmetry
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    symmetryLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    symmetrySlider.setBounds(row);
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 5: Waveform
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    waveformLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    waveformCombo.setBounds(row.removeFromLeft(150));
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 6: Sync Rate
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    syncRateLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
    row.removeFromLeft(SPACING);
    syncRateCombo.setBounds(row.removeFromLeft(150));
    currentY += ROW_HEIGHT + SPACING;
    
    // Row 7: Toggle buttons
    row = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), ROW_HEIGHT);
    enabledButton.setBounds(row.removeFromLeft(80));
    row.removeFromLeft(SPACING);
    invertButton.setBounds(row.removeFromLeft(80));
    row.removeFromLeft(SPACING);
    syncToHostButton.setBounds(row.removeFromLeft(120));
}

void LFOComponent::updateFromLFO()
{
    updateControlsFromLFO();
}

void LFOComponent::setFrequencyRange(double minFreq, double maxFreq, double interval)
{
    frequencySlider.setRange(minFreq, maxFreq, interval);
}

void LFOComponent::setFrequencySkewFactor(double skewFactor)
{
    frequencySlider.setSkewFactorFromMidPoint(skewFactor);
}

void LFOComponent::setSyncToHostEnabled(bool enabled)
{
    syncToHostButton.setEnabled(enabled);
    syncRateCombo.setEnabled(enabled);
    
    // If disabling, also set the LFO to not sync to host
    if (!enabled)
    {
        lfo.setSyncToHost(false);
        syncToHostButton.setToggleState(false, juce::dontSendNotification);
    }
}

void LFOComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &frequencySlider) {
        lfo.setFrequency(slider->getValue());
        DBG("UI: LFO Frequency changed to: " + juce::String(slider->getValue()) + " Hz, LFO address: " + juce::String::toHexString(reinterpret_cast<juce::uint64>(&lfo)));
        DBG("UI: After setting - LFO reports frequency: " + juce::String(lfo.getFrequency()));
    }
    else if (slider == &depthSlider) {
        lfo.setDepth(static_cast<float>(slider->getValue()));
        DBG("UI: LFO Depth changed to: " + juce::String(slider->getValue()) + ", LFO address: " + juce::String::toHexString(reinterpret_cast<juce::uint64>(&lfo)));
        DBG("UI: After setting - LFO reports depth: " + juce::String(lfo.getDepth()));
    }
    else if (slider == &phaseOffsetSlider) {
        // Convert degrees to radians
        lfo.setPhaseOffset(slider->getValue() * juce::MathConstants<double>::pi / 180.0);
        DBG("UI: LFO Phase changed to: " + juce::String(slider->getValue()) + " degrees, LFO address: " + juce::String::toHexString(reinterpret_cast<juce::uint64>(&lfo)));
    }
    else if (slider == &symmetrySlider) {
        // Convert percentage to 0-1 range
        lfo.setSymmetry(static_cast<float>(slider->getValue() / 100.0));
        DBG("UI: LFO Symmetry changed to: " + juce::String(slider->getValue()) + "%, LFO address: " + juce::String::toHexString(reinterpret_cast<juce::uint64>(&lfo)));
    }
}

void LFOComponent::buttonClicked(juce::Button* button)
{
    if (button == &enabledButton) {
        lfo.setEnabled(button->getToggleState());
    }
    else if (button == &invertButton) {
        lfo.setInvert(button->getToggleState());
    }
    else if (button == &syncToHostButton) {
        lfo.setSyncToHost(button->getToggleState());
    }
}

void LFOComponent::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &waveformCombo) {
        lfo.setWaveShape(static_cast<LFO::WaveformType>(combo->getSelectedId() - 1));
    }
    else if (combo == &syncRateCombo) {
        lfo.setSyncRate(combo->getSelectedId() - 1);
    }
}

} // namespace audio_plugin 