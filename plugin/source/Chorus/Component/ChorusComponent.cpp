#include "ChorusComponent.h"

namespace audio_plugin {

ChorusComponent::ChorusComponent(Chorus& chorusRef, const juce::String& componentName)
    : chorus(chorusRef)
{
    setName(componentName);
    
    // Setup global controls first
    setupGlobalControls();
    
    // Create scrollable viewport for LFO controls
    viewport = std::make_unique<juce::Viewport>();
    contentComponent = std::make_unique<juce::Component>();
    
    viewport->setViewedComponent(contentComponent.get(), false);
    addAndMakeVisible(viewport.get());
    
    createLFOUIs();
    
    // Initialize controls with current chorus values
    updateGlobalControlsFromChorus();
    updateLFOEnableState();
    
    setSize(800, 600);
}

ChorusComponent::~ChorusComponent()
{
    lfoComponents.clear();
    viewport.reset();
    contentComponent.reset();
}

void ChorusComponent::createLFOUIs()
{
    lfoComponents.clear();
    
    // Create LFO UIs for each voice (each voice has 2 LFOs: left/mid and right/side)
    int maxVoices = chorus.getMaxVoices();
    
    for (int voiceIndex = 0; voiceIndex < maxVoices; ++voiceIndex)
    {
        // Left/Mid/Mono LFO
        if (auto* leftLFO = chorus.getVoiceLeftLFO(voiceIndex))
        {
            auto* leftLFOComponent = lfoComponents.add(new LFOComponent(*leftLFO, 
                "Voice " + juce::String(voiceIndex + 1) + " Left/Mid/Mono LFO"));
            contentComponent->addAndMakeVisible(leftLFOComponent);
        }
        
        // Right/Side LFO
        if (auto* rightLFO = chorus.getVoiceRightLFO(voiceIndex))
        {
            auto* rightLFOComponent = lfoComponents.add(new LFOComponent(*rightLFO, 
                "Voice " + juce::String(voiceIndex + 1) + " Right/Side LFO"));
            contentComponent->addAndMakeVisible(rightLFOComponent);
        }
    }
    
    // Configure all LFO components for chorus-specific settings
    configureChorusLFOs();
}

void ChorusComponent::configureChorusLFOs()
{
    // Configure each LFO component for chorus-specific requirements
    for (auto* lfoComponent : lfoComponents)
    {
        if (lfoComponent != nullptr)
        {
            // Limit frequency range to 20Hz max for chorus LFOs
            lfoComponent->setFrequencyRange(0.001, 20.0, 0.001);
            lfoComponent->setFrequencySkewFactor(2.0); // Adjust midpoint for 20Hz range
            
            // Disable sync to host for chorus LFOs
            lfoComponent->setSyncToHostEnabled(false);
        }
    }
}

void ChorusComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Chorus Controls", getLocalBounds().removeFromTop(30), 
               juce::Justification::centred, true);
    
    // Draw separator line between global controls and LFO section
    g.setColour(juce::Colours::lightgrey);
    int separatorY = 30 + GLOBAL_SECTION_HEIGHT;
    g.drawHorizontalLine(separatorY, 0.0f, static_cast<float>(getWidth()));
    
    // Global controls section background
    auto globalBounds = juce::Rectangle<int>(0, 30, getWidth(), GLOBAL_SECTION_HEIGHT);
    g.setColour(juce::Colour::fromRGB(60, 60, 65));
    g.fillRect(globalBounds);
    
    // Global controls title
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Global Chorus Parameters", globalBounds.removeFromTop(25), 
               juce::Justification::centred, true);
}

void ChorusComponent::resized()
{
    auto bounds = getLocalBounds();
    auto titleBounds = bounds.removeFromTop(30); // Space for title
    
    // Layout global controls at the top
    auto globalBounds = bounds.removeFromTop(GLOBAL_SECTION_HEIGHT);
    layoutGlobalControls(globalBounds);
    
    // Set viewport to fill remaining space
    viewport->setBounds(bounds);
    
    int numComponents = lfoComponents.size();
    if (numComponents == 0) return;
    
    // Two-column flexbox layout within the content component
    int columnWidth = bounds.getWidth() / 2;
    int leftColumnY = 0;
    int rightColumnY = 0;
    int margin = 5;
    
    for (int i = 0; i < numComponents; ++i)
    {
        auto* lfoComponent = lfoComponents[i];
        
        // Use a fixed height based on LFO component's known layout needs
        // LFOComponent has: frequency, depth, phase, symmetry, waveform, sync controls
        // Each row is ~30px + margins, so about 8-9 rows = ~300px
        int preferredHeight = 300;
        
        // Alternate between left and right columns
        bool isLeftColumn = (i % 2 == 0);
        
        if (isLeftColumn)
        {
            lfoComponent->setBounds(0, leftColumnY, columnWidth - margin, preferredHeight);
            leftColumnY += preferredHeight + margin;
        }
        else
        {
            lfoComponent->setBounds(columnWidth, rightColumnY, columnWidth - margin, preferredHeight);
            rightColumnY += preferredHeight + margin;
        }
    }
    
    // Set content component size to fit all LFO components
    int totalRequiredHeight = juce::jmax(leftColumnY, rightColumnY);
    contentComponent->setSize(bounds.getWidth(), totalRequiredHeight);
}

void ChorusComponent::updateFromChorus()
{
    // Update global controls
    updateGlobalControlsFromChorus();
    
    // Update all LFO UIs from their corresponding LFOs
    for (auto* lfoComponent : lfoComponents)
    {
        lfoComponent->updateFromLFO();
    }
}

void ChorusComponent::refreshLFOUIs()
{
    createLFOUIs();
    resized();
}

void ChorusComponent::setupGlobalControls()
{
    // === MIX ===
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    mixSlider.addListener(this);
    addAndMakeVisible(mixSlider);
    
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mixLabel);
    
    // === BASE DELAY ===
    baseDelaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    baseDelaySlider.setRange(10.0, 100.0, 1.0);
    baseDelaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    baseDelaySlider.setTextValueSuffix(" ms");
    baseDelaySlider.addListener(this);
    addAndMakeVisible(baseDelaySlider);
    
    baseDelayLabel.setText("Base Delay", juce::dontSendNotification);
    baseDelayLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(baseDelayLabel);
    
    // === ENABLED ===
    enabledButton.setButtonText("Enabled");
    enabledButton.addListener(this);
    addAndMakeVisible(enabledButton);
    
    // === STEREO MODE ===
    stereoModeCombo.addItem("Mono", 1);
    stereoModeCombo.addItem("Stereo", 2);
    stereoModeCombo.addItem("Mid-Side", 3);
    stereoModeCombo.addListener(this);
    addAndMakeVisible(stereoModeCombo);
    
    stereoModeLabel.setText("Stereo Mode", juce::dontSendNotification);
    stereoModeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stereoModeLabel);
    
    // === STEREO SPREAD ===
    stereoSpreadSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    stereoSpreadSlider.setRange(0.0, 1.0, 0.01);
    stereoSpreadSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    stereoSpreadSlider.addListener(this);
    addAndMakeVisible(stereoSpreadSlider);
    
    stereoSpreadLabel.setText("Stereo Spread", juce::dontSendNotification);
    stereoSpreadLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stereoSpreadLabel);
    
    // === MID/SIDE ENABLED ===
    midEnabledButton.setButtonText("Mid Enabled");
    midEnabledButton.addListener(this);
    addAndMakeVisible(midEnabledButton);
    
    sideEnabledButton.setButtonText("Side Enabled");
    sideEnabledButton.addListener(this);
    addAndMakeVisible(sideEnabledButton);
    
    // === SIDE GAIN ===
    sideGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sideGainSlider.setRange(-20.0, 20.0, 0.1);
    sideGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    sideGainSlider.setTextValueSuffix(" dB");
    sideGainSlider.addListener(this);
    addAndMakeVisible(sideGainSlider);
    
    sideGainLabel.setText("Side Gain", juce::dontSendNotification);
    sideGainLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sideGainLabel);
}

void ChorusComponent::updateGlobalControlsFromChorus()
{
    mixSlider.setValue(chorus.getMix(), juce::dontSendNotification);
    baseDelaySlider.setValue(chorus.getBaseDelay(), juce::dontSendNotification);
    enabledButton.setToggleState(chorus.isEnabled(), juce::dontSendNotification);
    
    // Set stereo mode combo
    switch (chorus.getStereoMode()) {
        case Chorus::StereoMode::Mono: stereoModeCombo.setSelectedId(1, juce::dontSendNotification); break;
        case Chorus::StereoMode::Stereo: stereoModeCombo.setSelectedId(2, juce::dontSendNotification); break;
        case Chorus::StereoMode::MidSide: stereoModeCombo.setSelectedId(3, juce::dontSendNotification); break;
    }
    
    stereoSpreadSlider.setValue(chorus.getStereoSpread(), juce::dontSendNotification);
    midEnabledButton.setToggleState(chorus.isMidEnabled(), juce::dontSendNotification);
    sideEnabledButton.setToggleState(chorus.isSideEnabled(), juce::dontSendNotification);
    sideGainSlider.setValue(chorus.getSideGain(), juce::dontSendNotification);
}

void ChorusComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &mixSlider) {
        chorus.setMix(static_cast<float>(slider->getValue()));
    }
    else if (slider == &baseDelaySlider) {
        chorus.setBaseDelay(static_cast<float>(slider->getValue()));
    }
    else if (slider == &stereoSpreadSlider) {
        chorus.setStereoSpread(static_cast<float>(slider->getValue()));
    }
    else if (slider == &sideGainSlider) {
        chorus.setSideGain(static_cast<float>(slider->getValue()));
    }
}

void ChorusComponent::buttonClicked(juce::Button* button)
{
    if (button == &enabledButton) {
        chorus.setEnabled(button->getToggleState());
    }
    else if (button == &midEnabledButton) {
        chorus.setMidEnabled(button->getToggleState());
    }
    else if (button == &sideEnabledButton) {
        chorus.setSideEnabled(button->getToggleState());
    }
}

void ChorusComponent::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &stereoModeCombo) {
        switch (combo->getSelectedId()) {
            case 1: 
                chorus.setStereoMode(Chorus::StereoMode::Mono);
                updateLFOEnableState(); // Update LFO enable state
                break;
            case 2: 
                chorus.setStereoMode(Chorus::StereoMode::Stereo);
                updateLFOEnableState(); // Update LFO enable state
                break;
            case 3: 
                chorus.setStereoMode(Chorus::StereoMode::MidSide);
                updateLFOEnableState(); // Update LFO enable state
                break;
        }
    }
}

void ChorusComponent::layoutGlobalControls(juce::Rectangle<int> bounds)
{
    bounds.reduce(MARGIN, MARGIN);
    
    // Use flexbox for cleaner vertical layout
    juce::FlexBox flexBox;
    flexBox.flexDirection = juce::FlexBox::Direction::column;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
    
    // Helper to layout a row with label and control
    auto layoutRow = [&](juce::Rectangle<int> rowBounds, juce::Component& label, juce::Component& control, int controlWidth = CONTROL_WIDTH) {
        label.setBounds(rowBounds.removeFromLeft(LABEL_WIDTH));
        rowBounds.removeFromLeft(SPACING);
        control.setBounds(rowBounds.removeFromLeft(controlWidth));
    };
    
    // Helper to layout mix row with button
    auto layoutMixRow = [&](juce::Rectangle<int> rowBounds) {
        mixLabel.setBounds(rowBounds.removeFromLeft(LABEL_WIDTH));
        rowBounds.removeFromLeft(SPACING);
        mixSlider.setBounds(rowBounds.removeFromLeft(CONTROL_WIDTH));
        rowBounds.removeFromLeft(SPACING * 2);
        enabledButton.setBounds(rowBounds.removeFromLeft(100));
    };
    
    // Helper to layout button row
    auto layoutButtonRow = [&](juce::Rectangle<int> rowBounds) {
        midEnabledButton.setBounds(rowBounds.removeFromLeft(120));
        rowBounds.removeFromLeft(SPACING);
        sideEnabledButton.setBounds(rowBounds.removeFromLeft(120));
    };
    
    // Calculate row positions using flexbox spacing
    int totalRows = 6;
    int totalSpacing = (totalRows - 1) * SPACING;
    int availableHeight = bounds.getHeight() - totalSpacing;
    int rowHeight = availableHeight / totalRows;
    
    int currentY = bounds.getY();
    
    // Row 1: Mix + Enabled
    auto mixRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutMixRow(mixRow);
    currentY += rowHeight + SPACING;
    
    // Row 2: Base Delay
    auto baseDelayRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutRow(baseDelayRow, baseDelayLabel, baseDelaySlider);
    currentY += rowHeight + SPACING;
    
    // Row 3: Stereo Mode
    auto stereoModeRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutRow(stereoModeRow, stereoModeLabel, stereoModeCombo, 150);
    currentY += rowHeight + SPACING;
    
    // Row 4: Stereo Spread
    auto stereoSpreadRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutRow(stereoSpreadRow, stereoSpreadLabel, stereoSpreadSlider);
    currentY += rowHeight + SPACING;
    
    // Row 5: Mid/Side buttons
    auto buttonsRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutButtonRow(buttonsRow);
    currentY += rowHeight + SPACING;
    
    // Row 6: Side Gain
    auto sideGainRow = juce::Rectangle<int>(bounds.getX(), currentY, bounds.getWidth(), rowHeight);
    layoutRow(sideGainRow, sideGainLabel, sideGainSlider);
}

void ChorusComponent::updateLFOEnableState()
{
    bool isMonoMode = (chorus.getStereoMode() == Chorus::StereoMode::Mono);
    
    // Enable/disable right column LFO components based on stereo mode
    // Right column components are at odd indices (1, 3, 5, etc.)
    for (int i = 0; i < lfoComponents.size(); ++i) {
        if (i % 2 == 1) { // Right column (odd indices)
            lfoComponents[i]->setEnabled(!isMonoMode);
        }
    }
}

} // namespace audio_plugin
