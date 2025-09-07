#include "Chorus.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DelayLine/DelayLine.h"

namespace WizardCore {

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

Chorus::Chorus(int maxVoicesIn) 
    : maxVoices(juce::jlimit(MIN_MAX_VOICES, MAX_MAX_VOICES, maxVoicesIn)) {
    
    // Allocate voices array
    voices = std::make_unique<Voice[]>(maxVoices);
    
    // Initialize all voices with default values
    for (int i = 0; i < maxVoices; ++i) {
        initializeVoice(i);
    }
    
    // Initialize stereo configuration
    updateStereoConfiguration();
}

Chorus::~Chorus() {
    // Destructor - no cleanup needed as RAII handles everything
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void Chorus::prepare(double sampleRateIn, int numChannels) {
    if (sampleRateIn <= 0.0 || numChannels <= 0) {
        DBG("Chorus: Invalid parameters in prepare()");
        return;
    }
    
    sampleRate = sampleRateIn;
    
    // Prepare each voice
    for (int i = 0; i < maxVoices; ++i) {
        Voice& voice = voices[i];
        
        // Prepare both LFOs with default chorus settings
        for (int lfoIndex = 0; lfoIndex < 2; ++lfoIndex) {
            voice.lfos[lfoIndex].prepare(sampleRateIn);
            
            // Set LFO to sine wave with typical chorus settings
            voice.lfos[lfoIndex].setWaveShape(LFO::WaveShape::Sine);
            voice.lfos[lfoIndex].setInvert(false);
            voice.lfos[lfoIndex].setSymmetry(0.5f);  // 50% = symmetric
            voice.lfos[lfoIndex].setSyncToHost(false);
            voice.lfos[lfoIndex].setCoupling(LFO::CouplingType::AC);  // Use AC coupling for bipolar output [-1,1]
        }
        
        // Prepare delay lines with maximum delay time needed
        // Base delay + max modulation depth = max possible delay
        double maxDelayTime = (voice.baseDelayMs.load() + MAX_DELAY_MS) * 0.001; // Convert ms to seconds
        
        // Prepare both delay lines independently
        for (int channel = 0; channel < 2; ++channel) {
            // Create delay line using factory based on current setting
            voice.delayLines[channel] = DelayLine::create(config.delayType.load());
            
            voice.delayLines[channel]->prepare(sampleRateIn, maxDelayTime, 1);  // Mono delay line per channel
            voice.delayLines[channel]->setDelayTimeInSeconds(voice.baseDelayMs.load() * 0.001); // Set initial delay time in seconds
            
            // Set longer smoothing time for low base delays to prevent warbling
            double smoothingTime = voice.baseDelayMs.load() < 45.0f ? 0.1 : 0.05; // 100ms vs 50ms
            voice.delayLines[channel]->setSmoothingTime(smoothingTime);
        }
    }
    
    // Prepare filters for wet signal processing
    for (int i = 0; i < 2; ++i) {
        // Prepare LPF and HPF for each channel
        lpfFilters[i].reset();
        hpfFilters[i].reset();
        
        // Set filter coefficients (will be updated when parameters change)
        updateLPFCoefficients(i);
        updateHPFCoefficients(i);
    }
    
    prepared = true;
}

void Chorus::processBlock(juce::AudioBuffer<float>& buffer) {
    if (!prepared.load()) {
        DBG("Chorus: Not prepared, skipping processing");
        return;
    }
    
    if (!config.enabled.load()) {
        // Chorus is disabled, just pass through the audio unchanged
        return;
    }
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples <= 0 || numChannels <= 0) {
        return;
    }
    
    // Create a temporary buffer for the wet signal
    juce::AudioBuffer<float> wetBuffer(numChannels, numSamples);
    wetBuffer.clear();
    
    // Switch between mono, stereo, and mid-side processing based on current mode
    PanningMode currentMode = config.panningMode.load();
    switch (currentMode) {
        case PanningMode::Mono:
            processVoicesMono(buffer, wetBuffer);
            break;
        case PanningMode::Stereo:
            processVoicesStereo(buffer, wetBuffer);
            break;
        case PanningMode::MidSide:
            processVoicesMidSide(buffer, wetBuffer);
            break;
        default:
            processVoicesMono(buffer, wetBuffer);  // Fallback to mono
            break;
    }
    
    // Apply filters to the wet signal if enabled
    processFilters(wetBuffer);
    
    // Normalize wet buffer by number of active voices to prevent volume buildup
    int activeVoiceCount = 0;
    for (int i = 0; i < maxVoices; ++i) {
        if (voices[i].enabled.load()) {
            activeVoiceCount++;
        }
    }
    
    if (activeVoiceCount > 0) {
        // Use a gentler logarithmic normalization that preserves more of the wet signal
        // This prevents the effect from being too quiet at 50% mix
        float logNormalization = 1.0f / (0.5f + std::log10(static_cast<float>(activeVoiceCount) + 0.5f));
        for (int channel = 0; channel < numChannels; ++channel) {
            float* wetChannelData = wetBuffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample) {
                wetChannelData[sample] *= logNormalization;
            }
        }
    }
    
    // Apply master voice level gain to the entire wet buffer
    float masterGain = juce::Decibels::decibelsToGain(config.masterVoiceLevelDb.load());
    for (int channel = 0; channel < numChannels; ++channel) {
        float* wetChannelData = wetBuffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample) {
            wetChannelData[sample] *= masterGain;
        }
    }
    
    // Mix dry and wet signals using equal-power crossfade
    for (int channel = 0; channel < numChannels; ++channel) {
        float* channelData = buffer.getWritePointer(channel);
        const float* wetChannelData = wetBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample) {
            float mixValue = config.mix.load();
            
            // Equal Power crossfade ensures constant energy across the mix range
            // This eliminates the volume drop at 50% mix
            float dryMix = std::cos(mixValue * juce::MathConstants<float>::halfPi);
            float wetMix = std::sin(mixValue * juce::MathConstants<float>::halfPi);
            
            // Apply the crossfade
            channelData[sample] = channelData[sample] * dryMix + wetChannelData[sample] * wetMix;
        }
    }
}

void Chorus::clear() {
    if (prepared.load()) {
        for (int i = 0; i < maxVoices; ++i) {
            for (int channel = 0; channel < 2; ++channel) {
                voices[i].delayLines[channel]->clear();
            }
                    // Reset both LFOs
        voices[i].lfos[0].reset();
        voices[i].lfos[1].reset();
        }
    }
}

// ============================================================================
// INDIVIDUAL PARAMETER SETTERS
// ============================================================================

void Chorus::setEnabled(bool enabled) {
    config.enabled.store(enabled);
}

void Chorus::setNumVoices(int numVoices) {
    if (numVoices < 1 || numVoices > maxVoices) {
        DBG("Chorus: Invalid number of voices: " << numVoices << " (must be 1-" << maxVoices << ")");
        return;
    }
    
    // Enable the first numVoices voices, disable the rest
    for (int i = 0; i < maxVoices; ++i) {
        bool voiceEnabled = (i < numVoices);
        voices[i].enabled = voiceEnabled;
        
        // Also update the LFO enabled state to match the voice enabled state
        voices[i].lfos[0].setEnabled(voiceEnabled);
        voices[i].lfos[1].setEnabled(voiceEnabled);
    }
    
    config.numActiveVoices.store(numVoices);
}

void Chorus::setRate(float rate) {
    // Update LFO frequency for all active voices
    if (prepared.load()) {
        int currentVoiceCount = config.numActiveVoices.load();
        for (int i = 0; i < currentVoiceCount; ++i) {
            voices[i].lfos[0].setFrequency(rate);
            voices[i].lfos[1].setFrequency(rate);
        }
    }
    config.rate.store(rate);
}

void Chorus::setDepth(float depth) {
    // Update LFO depth for all active voices
    if (prepared.load()) {
        int currentVoiceCount = config.numActiveVoices.load();
        for (int i = 0; i < currentVoiceCount; ++i) {
            voices[i].lfos[0].setDepth(depth);
            voices[i].lfos[1].setDepth(depth);
        }
    }
    config.depth.store(depth);
}

void Chorus::setMix(float mixIn) {
    if (mixIn < MIN_MIX || mixIn > MAX_MIX) {
        DBG("Chorus: Mix out of range: " << mixIn);
        return;
    }
    
    config.mix.store(mixIn);
}

void Chorus::setBaseDelay(float delayMs) {
    if (delayMs < MIN_BASE_DELAY || delayMs > MAX_BASE_DELAY) {
        DBG("Chorus: Base delay out of range: " << delayMs << " ms");
        return;
    }
    
    config.baseDelay.store(delayMs);
    
    // Update voices based on current count, not enabled state
    int currentVoiceCount = config.numActiveVoices.load();
    for (int i = 0; i < currentVoiceCount; ++i) {
        voices[i].baseDelayMs = delayMs;
        if (prepared.load()) {
            for (int channel = 0; channel < 2; ++channel) {
                voices[i].delayLines[channel]->setDelayTimeInSeconds(delayMs * 0.001); // Convert to seconds
            }
        }
    }
}

void Chorus::setVoiceAttenuation(float attenuationDb) {
    if (attenuationDb < MIN_VOICE_ATTENUATION || attenuationDb > MAX_VOICE_ATTENUATION) {
        DBG("Chorus: Voice attenuation out of range: " << attenuationDb << " dB (must be " << MIN_VOICE_ATTENUATION << " to " << MAX_VOICE_ATTENUATION << " dB)");
        return;
    }
    
    config.voiceAttenuationDb.store(attenuationDb);
}

void Chorus::setMasterVoiceLevel(float levelDb) {
    if (levelDb < MIN_MASTER_VOICE_LEVEL || levelDb > MAX_MASTER_VOICE_LEVEL) {
        DBG("Chorus: Master voice level out of range: " << levelDb << " dB (must be " << MIN_MASTER_VOICE_LEVEL << " to " << MAX_MASTER_VOICE_LEVEL << " dB)");
        return;
    }
    
    config.masterVoiceLevelDb.store(levelDb);
}

void Chorus::setStereoMode(PanningMode mode) {
    config.panningMode.store(mode);
    updateStereoConfiguration();
    
    // In mono mode, sync right LFOs to left LFOs
    if (mode == PanningMode::Mono) {
        syncRightLFOsToLeft();
    }
}

void Chorus::setStereoSpread(float spread) {
    config.stereoSpread.store(std::clamp(spread, 0.0f, 1.0f));
    updateStereoConfiguration();
}

void Chorus::setMidEnabled(bool enabled) {
    config.midEnabled.store(enabled);
}

void Chorus::setSideEnabled(bool enabled) {
    config.sideEnabled.store(enabled);
}

void Chorus::setSideGain(float gainDb) {
    config.sideGainDb.store(std::clamp(gainDb, -20.0f, 20.0f));
}

void Chorus::setLPFEnabled(bool enabled) {
    config.lpfEnabled.store(enabled);
}

void Chorus::setHPFEnabled(bool enabled) {
    config.hpfEnabled.store(enabled);
}

void Chorus::setLPFCutoff(float frequencyHz) {
    config.lpfCutoff.store(std::clamp(frequencyHz, 20.0f, 20000.0f));
    
    // Update LPF coefficients for both channels when cutoff changes
    if (prepared.load()) {
        for (int i = 0; i < 2; ++i) {
            updateLPFCoefficients(i);
        }
    }
}

void Chorus::setHPFCutoff(float frequencyHz) {
    config.hpfCutoff.store(std::clamp(frequencyHz, 20.0f, 20000.0f));
    
    // Update HPF coefficients for both channels when cutoff changes
    if (prepared.load()) {
        for (int i = 0; i < 2; ++i) {
            updateHPFCoefficients(i);
        }
    }
}

void Chorus::setDelayType(DelayType delayType) {
    config.delayType.store(delayType);
    
    // If already prepared, recreate delay lines with new type
    if (prepared.load()) {
        for (int i = 0; i < maxVoices; ++i) {
            for (int ch = 0; ch < 2; ++ch) {
                // Store current delay time safely
                double currentDelay = 0.0;
                if (voices[i].delayLines[ch] && voices[i].delayLines[ch]->isPrepared()) {
                    currentDelay = voices[i].delayLines[ch]->getDelayTimeInSeconds(); // Get delay time in seconds
                } else {
                    // Use base delay if current delay line isn't prepared
                    currentDelay = voices[i].baseDelayMs.load() * 0.001;
                }
                
                // Create new delay line of the specified type
                voices[i].delayLines[ch] = DelayLine::create(delayType);
                
                // Re-prepare with same settings
                double maxDelayTime = (voices[i].baseDelayMs.load() + MAX_DELAY_MS) * 0.001;
                voices[i].delayLines[ch]->prepare(sampleRate.load(), maxDelayTime, 1);
                
                // Set delay time
                voices[i].delayLines[ch]->setDelayTimeInSeconds(currentDelay); // Set delay time in seconds
            }
        }
    }
}

// Per-voice parameter setters
void Chorus::setVoiceEnabled(int voiceIndex, bool enabled) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    voices[voiceIndex].enabled = enabled;
    
    // Also update the LFO enabled state to match the voice enabled state
    voices[voiceIndex].lfos[0].setEnabled(enabled);
    voices[voiceIndex].lfos[1].setEnabled(enabled);
}



void Chorus::setVoiceBaseDelay(int voiceIndex, float delayMs) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (delayMs < MIN_BASE_DELAY || delayMs > MAX_BASE_DELAY) {
        DBG("Chorus: Base delay out of range: " << delayMs << " ms");
        return;
    }
    
                voices[voiceIndex].baseDelayMs = delayMs;
    
    // Update current delay time if already prepared
    if (prepared.load()) {
        for (int channel = 0; channel < 2; ++channel) {
            voices[voiceIndex].delayLines[channel]->setDelayTimeInSeconds(delayMs * 0.001); // Convert to seconds
        }
    }
}

// ============================================================================
// INDIVIDUAL PARAMETER GETTERS
// ============================================================================

bool Chorus::isVoiceEnabled(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return false;
    }
    return voices[voiceIndex].enabled.load();
}

float Chorus::getVoiceRate(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return 0.0f;
    }
    // Return frequency from left LFO
    return static_cast<float>(voices[voiceIndex].lfos[0].getFrequency());
}

float Chorus::getVoiceDepth(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return 0.0f;
    }
    // Return depth from left LFO
    return voices[voiceIndex].lfos[0].getDepth();
}



float Chorus::getVoiceBaseDelay(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return 0.0f;
    }
    return voices[voiceIndex].baseDelayMs.load();
}

float Chorus::getVoicePhaseOffset(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return 0.0f;
    }
    // Return phase offset from left LFO (convert from radians to degrees)
    return static_cast<float>(voices[voiceIndex].lfos[0].getPhaseOffsetRadians() * 180.0 / juce::MathConstants<double>::pi);
}

// ============================================================================
// LFO AND DELAY LINE ACCESS
// ============================================================================

LFO* Chorus::getVoiceLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return nullptr;
    }
    // Return left/mid LFO for backward compatibility
    return &voices[voiceIndex].lfos[0];
}

LFO* Chorus::getVoiceLeftLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return nullptr;
    }
    return &voices[voiceIndex].lfos[0];
}

LFO* Chorus::getVoiceRightLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        return nullptr;
    }
    return &voices[voiceIndex].lfos[1];
}

DelayLine* Chorus::getVoiceDelayLine(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= maxVoices) {
        DBG("Chorus: Invalid voice index for DelayLine access: " << voiceIndex);
        return nullptr;
    }
    return voices[voiceIndex].delayLines[0].get();
}

// ============================================================================
// BATCH PARAMETER UPDATES
// ============================================================================

void Chorus::updateParameters(float rate, float depth, float mix, float baseDelay, float voiceAttenuation, float masterVoiceLevel, int voiceCount,
                             int panningMode, float stereoSpread,
                             bool midEnabled, bool sideEnabled, float sideGain,
                             bool lpfEnabled, float lpfCutoff,
                             bool hpfEnabled, float hpfCutoff) {
    // Update all parameters using individual setter methods
    setRate(rate);
    setDepth(depth);
    setMix(mix);
    setBaseDelay(baseDelay);
    setVoiceAttenuation(voiceAttenuation);
    setMasterVoiceLevel(masterVoiceLevel);
    setNumVoices(voiceCount);
    setStereoMode(static_cast<PanningMode>(panningMode));
    setStereoSpread(stereoSpread);
    setMidEnabled(midEnabled);
    setSideEnabled(sideEnabled);
    setSideGain(sideGain);
    setLPFEnabled(lpfEnabled);
    setLPFCutoff(lpfCutoff);
    setHPFEnabled(hpfEnabled);
    setHPFCutoff(hpfCutoff);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void Chorus::initializeVoice(int voiceIndex) {
    Voice& voice = voices[voiceIndex];
    
    // Set voice-specific parameters using dedicated methods
    setVoiceEnabled(voiceIndex, voiceIndex < config.numActiveVoices.load());
    setVoiceBaseDelay(voiceIndex, config.baseDelay.load() + (voiceIndex * 3.0f)); // Slightly different delays for each voice
    
    // Initialize LFO objects with default parameters from config
    float voiceRateHz = config.rate.load() + (voiceIndex * 0.15f); // Slightly different rates for each voice
    float voicePhaseDegrees = maxVoices > 1 ? voiceIndex * (360.0f / maxVoices) : 0.0f; // Evenly distribute phases
    
    // Set LFO parameters using dedicated methods
    for (int lfoIndex = 0; lfoIndex < 2; ++lfoIndex) {
        voice.lfos[lfoIndex].setFrequency(voiceRateHz);
        voice.lfos[lfoIndex].setDepth(config.depth.load());
        voice.lfos[lfoIndex].setPhaseOffsetRadians(voicePhaseDegrees * juce::MathConstants<double>::pi / 180.0);
        voice.lfos[lfoIndex].setEnabled(voice.enabled.load());
    }
    
    // Initialize delay lines with default type from config
    for (int channel = 0; channel < 2; ++channel) {
        voice.delayLines[channel] = DelayLine::create(config.delayType.load());
    }
}



void Chorus::syncRightLFOsToLeft() {
    // Copy all left LFO parameters to right LFOs for mono mode
    for (int i = 0; i < config.numActiveVoices.load(); ++i) {
        Voice& voice = voices[i];
        
        // Copy all parameters from left LFO (index 0) to right LFO (index 1)
        voice.lfos[1].setFrequency(voice.lfos[0].getFrequency());
        voice.lfos[1].setDepth(voice.lfos[0].getDepth());
        voice.lfos[1].setPhaseOffsetRadians(voice.lfos[0].getPhaseOffsetRadians());
        voice.lfos[1].setSymmetry(voice.lfos[0].getSymmetry());
        voice.lfos[1].setWaveShape(voice.lfos[0].getWaveShape());
        voice.lfos[1].setInvert(voice.lfos[0].getInvert());
        voice.lfos[1].setSyncToHost(voice.lfos[0].getSyncToHost());
        voice.lfos[1].setEnabled(voice.lfos[0].isEnabled());
        
        // Reset the right LFO position to match left LFO
        voice.lfos[1].reset();
    }
}

void Chorus::updateStereoConfiguration() {
    PanningMode currentMode = config.panningMode.load();
    switch (currentMode) {
        case PanningMode::Mono:
            configureMonoPhaseOffsets();
            break;
        case PanningMode::Stereo:
            configureStereoPhaseOffsets();
            break;
        case PanningMode::MidSide:
            configureMidSidePhaseOffsets();
            break;
        default:
            configureMonoPhaseOffsets();  // Fallback to mono
            break;
    }
}

void Chorus::configureMonoPhaseOffsets() {
    // In mono mode, ensure no phase offsets
    for (int i = 0; i < config.numActiveVoices.load(); ++i) {
        Voice& voice = voices[i];
        // Set zero phase offset on both LFO objects
        voice.lfos[0].setPhaseOffsetRadians(0.0);
        voice.lfos[1].setPhaseOffsetRadians(0.0);
    }
}

void Chorus::configureStereoPhaseOffsets() {
    // Use full 360° range for maximum stereo effect
    // Apply a non-linear curve to the spread parameter to make it more dramatic
    float spreadCurve = std::pow(config.stereoSpread.load(), 0.5f); // Square root curve [0.0, 1.0] for more dramatic effect at lower values
    float maxPhaseOffsetDegrees = spreadCurve * 360.0f;  // 0° to 360° max
    
    for (int i = 0; i < config.numActiveVoices.load(); ++i) {
        Voice& voice = voices[i];
        
        // At 0% spread: both channels get 0° offset (mono)
        // At 100% spread: left = -180°, right = +180° (maximum separation)
        float leftPhaseOffsetDegrees = -maxPhaseOffsetDegrees * 0.5f;
        float rightPhaseOffsetDegrees = maxPhaseOffsetDegrees * 0.5f;
        
        // Add slight offset per voice for richer stereo field
        float voiceSpreadDegrees = (i * 15.0f) * spreadCurve; // Up to 15° additional spread per voice
        leftPhaseOffsetDegrees -= voiceSpreadDegrees;
        rightPhaseOffsetDegrees += voiceSpreadDegrees;
        
        // Set phase offsets directly on LFO objects (convert degrees to radians)
        voice.lfos[0].setPhaseOffsetRadians(leftPhaseOffsetDegrees * juce::MathConstants<double>::pi / 180.0);
        voice.lfos[1].setPhaseOffsetRadians(rightPhaseOffsetDegrees * juce::MathConstants<double>::pi / 180.0);
    }
}

void Chorus::configureMidSidePhaseOffsets() {
    // Use full 360° range for maximum mid-side effect
    // Apply a non-linear curve to the spread parameter to make it more dramatic
    float spreadCurve = std::pow(config.stereoSpread.load(), 0.5f); // Square root curve [0.0, 1.0] for more dramatic effect at lower values
    float maxPhaseOffsetDegrees = spreadCurve * 360.0f;  // 0° to 360° max
    
    for (int i = 0; i < config.numActiveVoices.load(); ++i) {
        Voice& voice = voices[i];
        
        // At 0% spread: both mid and side get 0° offset (mono)
        // At 100% spread: mid = -180°, side = +180° (maximum separation)
        float midPhaseOffsetDegrees = -maxPhaseOffsetDegrees * 0.5f;
        float sidePhaseOffsetDegrees = maxPhaseOffsetDegrees * 0.5f;
        
        // Add slight offset per voice for richer mid-side field
        float voiceSpreadDegrees = (i * 15.0f) * spreadCurve; // Up to 15° additional spread per voice
        midPhaseOffsetDegrees -= voiceSpreadDegrees;
        sidePhaseOffsetDegrees += voiceSpreadDegrees;
        
        // Set phase offsets directly on LFO objects (convert degrees to radians)
        voice.lfos[0].setPhaseOffsetRadians(midPhaseOffsetDegrees * juce::MathConstants<double>::pi / 180.0);
        voice.lfos[1].setPhaseOffsetRadians(sidePhaseOffsetDegrees * juce::MathConstants<double>::pi / 180.0);
    }
}

// ============================================================================
// STEREO PROCESSING METHODS
// ============================================================================

void Chorus::processVoicesMono(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples <= 0 || numChannels <= 0) {
        return;
    }
    
    // Process each enabled voice
    for (int voiceIndex = 0; voiceIndex < config.numActiveVoices.load(); ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get LFO value for this sample - use left/mid LFO for mono processing
            float lfoValue = voice.lfos[0].getNextSample();
            
            // Calculate modulated delay time using LFO's own depth parameter
            float lfoDepth = voice.lfos[0].getDepth();  // Get depth directly from LFO
            float modulationRange = lfoDepth * MAX_DELAY_MS * 0.001f; // Convert to seconds
            
            float modulatedDelay = voice.baseDelayMs.load() * 0.001f + lfoValue * modulationRange;
            
            // Clamp delay time to valid range
            modulatedDelay = std::max(0.001f, std::min(modulatedDelay, 0.1f)); // 1ms to 100ms
            
            // Get input samples for both channels
            const float* channelData[2] = { buffer.getReadPointer(0), buffer.getReadPointer(numChannels > 1 ? 1 : 0) };
            float* wetData[2] = { wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(numChannels > 1 ? 1 : 0) };
            
            // Process each channel with identical settings
            for (int channel = 0; channel < 2; ++channel) {
                voice.delayLines[channel]->setDelayTimeInSeconds(modulatedDelay); // Set delay time in seconds
                float delayedSample = voice.delayLines[channel]->processSample(0, channelData[channel][sample]);
                
                // Calculate voice gain in real-time based on index and attenuation
                float voiceGain = juce::Decibels::decibelsToGain(config.voiceAttenuationDb.load() * voiceIndex);
                wetData[channel][sample] += delayedSample * voiceGain;
            }
        }
    }
}

void Chorus::processVoicesStereo(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples <= 0 || numChannels <= 0) {
        return;
    }
    
    // Process each enabled voice
    for (int voiceIndex = 0; voiceIndex < config.numActiveVoices.load(); ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Get base delay and calculate modulation parameters
        float baseDelayMs = voice.baseDelayMs.load();
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get input samples for both channels
            const float* channelData[2] = { buffer.getReadPointer(0), buffer.getReadPointer(1) };
            float* wetData[2] = { wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1) };
            
            // Get LFO values for this sample - use independent LFOs for each channel
            float lfoValues[2];
            float depths[2];
            
            // Use independent LFOs for each channel
            lfoValues[0] = voice.lfos[0].getNextSample();
            lfoValues[1] = voice.lfos[1].getNextSample();
            depths[0] = voice.lfos[0].getDepth();  // Get depth from LFO object
            depths[1] = voice.lfos[1].getDepth();  // Get depth from LFO object

            // Process each channel with independent parameters
            for (int channel = 0; channel < 2; ++channel) {
                // Calculate modulation range
                float modulationRange = depths[channel] * MAX_DELAY_MS * 0.001f;
                
                // Adjust modulation range based on base delay to prevent artifacts
                if (baseDelayMs < 45.0f) {
                    float scaleFactor = juce::jmap(baseDelayMs, 10.0f, 45.0f, 0.4f, 1.0f);
                    modulationRange *= scaleFactor;
                }
                
                // Calculate delay time for this channel
                float baseDelaySec = baseDelayMs * 0.001f;
                float modulation = lfoValues[channel] * modulationRange;
                
                // Apply non-linear modulation scaling for smoother pitch variations
                modulation = std::copysign(std::pow(std::abs(modulation), 1.2f), modulation);
                
                float channelDelay = baseDelaySec + modulation;
                
                // Clamp delay time to valid range
                channelDelay = std::max(0.001f, std::min(channelDelay, 0.1f)); // 1ms to 100ms
                
                // Update delay time and process sample
                voice.delayLines[channel]->setDelayTimeInSeconds(channelDelay); // Set delay time in seconds
                float delayedSample = voice.delayLines[channel]->processSample(0, channelData[channel][sample]);
                
                // Calculate voice gain in real-time based on index and attenuation
                float voiceGain = juce::Decibels::decibelsToGain(config.voiceAttenuationDb.load() * voiceIndex);
                wetData[channel][sample] += delayedSample * voiceGain;
            }
        }
    }
}

void Chorus::processVoicesMidSide(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples <= 0 || numChannels < 2) {
        // Mid-side processing requires stereo input
        return;
    }
    
    // Create temporary buffers for mid-side conversion
    juce::AudioBuffer<float> midSideBuffer(2, numSamples);
    juce::AudioBuffer<float> midSideWetBuffer(2, numSamples);
    midSideBuffer.clear();
    midSideWetBuffer.clear();
    
    // Get pointers for mid and side channels
    const float* midSideData[2] = { midSideBuffer.getReadPointer(0), midSideBuffer.getReadPointer(1) };
    float* midSideWetData[2] = { midSideWetBuffer.getWritePointer(0), midSideWetBuffer.getWritePointer(1) };
    
    // Convert L/R to M/S
    for (int sample = 0; sample < numSamples; ++sample) {
        float left = buffer.getSample(0, sample);
        float right = buffer.getSample(1, sample);
        
        // M/S encoding: Mid = (L+R), Side = (L-R)
        // Note: We don't divide by 2 here to maintain proper amplitude
        float mid = left + right;
        float side = left - right;
        
        midSideBuffer.setSample(0, sample, mid);   // Mid channel
        midSideBuffer.setSample(1, sample, side);  // Side channel
    }
    
    // Process each enabled voice
    for (int voiceIndex = 0; voiceIndex < maxVoices; ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Get base delay and calculate modulation parameters
        float baseDelayMs = voice.baseDelayMs.load();
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get LFO values for this sample (one per LFO)
            float midLfoValue, sideLfoValue;
            float midDepth, sideDepth;
            
            // Use independent LFOs for mid and side channels
            midLfoValue = voice.lfos[0].getNextSample();  // Mid/left LFO
            sideLfoValue = voice.lfos[1].getNextSample(); // Side/right LFO
            midDepth = voice.lfos[0].getDepth();          // Get depth from LFO object
            sideDepth = voice.lfos[1].getDepth();         // Get depth from LFO object
            
            // Process mid channel (channel 0) if enabled
            if (config.midEnabled.load()) {
                // Calculate delay time for mid channel
                float modulationRange = midDepth * MAX_DELAY_MS * 0.001f;
                float baseDelaySec = baseDelayMs * 0.001f;
                float modulation = midLfoValue * modulationRange;
                
                // Apply non-linear modulation scaling for smoother pitch variations
                modulation = std::copysign(std::pow(std::abs(modulation), 1.2f), modulation);
                
                float midDelay = baseDelaySec + modulation;
                
                // Clamp delay time to valid range
                midDelay = std::max(0.001f, std::min(midDelay, 0.1f)); // 1ms to 100ms
                
                // Update delay time and process sample
                voice.delayLines[0]->setDelayTimeInSeconds(midDelay); // Set delay time in seconds
                float delayedSample = voice.delayLines[0]->processSample(0, midSideData[0][sample]);
                
                // Calculate voice gain in real-time based on index and attenuation
                float voiceGain = juce::Decibels::decibelsToGain(config.voiceAttenuationDb.load() * voiceIndex);
                midSideWetData[0][sample] += delayedSample * voiceGain;
            }
            
            // Process side channel (channel 1) if enabled
            if (config.sideEnabled.load()) {
                // Calculate delay time for side channel
                float modulationRange = sideDepth * MAX_DELAY_MS * 0.001f;
                float baseDelaySec = baseDelayMs * 0.001f;
                float modulation = sideLfoValue * modulationRange;
                
                // Apply non-linear modulation scaling for smoother pitch variations
                modulation = std::copysign(std::pow(std::abs(modulation), 1.2f), modulation);
                
                float sideDelay = baseDelaySec + modulation;
                
                // Clamp delay time to valid range
                sideDelay = std::max(0.001f, std::min(sideDelay, 0.1f)); // 1ms to 100ms
                
                // Update delay time and process sample
                voice.delayLines[1]->setDelayTimeInSeconds(sideDelay); // Set delay time in seconds
                float delayedSample = voice.delayLines[1]->processSample(0, midSideData[1][sample]);
                
                // Calculate voice gain in real-time based on index and attenuation
                float voiceGain = juce::Decibels::decibelsToGain(config.voiceAttenuationDb.load() * voiceIndex);
                
                // Add to wet buffer with configurable side gain
                // Convert dB to linear gain: gain = 10^(dB/20)
                float sideGainLinear = std::pow(10.0f, config.sideGainDb.load() / 20.0f);
                midSideWetData[1][sample] += delayedSample * voiceGain * sideGainLinear;
            }
        }
    }
    
    // Convert processed M/S back to L/R and add to wet buffer
    for (int sample = 0; sample < numSamples; ++sample) {
        // Get the original L/R samples for dry signal calculation
        float left = buffer.getSample(0, sample);
        float right = buffer.getSample(1, sample);
        
        // Calculate dry M/S signals
        float dryMid = left + right;
        float drySide = left - right;
        
        // Use processed signal for enabled channels, dry signal for disabled channels
        float finalMid = config.midEnabled.load() ? midSideWetData[0][sample] : dryMid;
        float finalSide = config.sideEnabled.load() ? midSideWetData[1][sample] : drySide;
        
        // M/S decoding: L = (M + S)/2, R = (M - S)/2
        float wetLeft = (finalMid + finalSide) * 0.5f;
        float wetRight = (finalMid - finalSide) * 0.5f;
        
        wetBuffer.addSample(0, sample, wetLeft);
        wetBuffer.addSample(1, sample, wetRight);
    }
}

// ============================================================================
// FILTER PROCESSING METHODS
// ============================================================================

void Chorus::updateLPFCoefficients(int channel) {
    if (channel < 0 || channel >= 2) return;
    
    // Calculate normalized frequency (0.0 to 1.0, where 1.0 = Nyquist)
    double normalizedFreq = config.lpfCutoff.load() / (sampleRate.load() * 0.5);
    normalizedFreq = std::clamp(normalizedFreq, 0.001, 0.99); // Prevent extreme values
    
    // Create LPF coefficients using JUCE's IIR filter design
    juce::IIRCoefficients coeffs = juce::IIRCoefficients::makeLowPass(sampleRate.load(), config.lpfCutoff.load(), 0.707f);
    
    // Apply coefficients to the LPF
    lpfFilters[channel].setCoefficients(coeffs);
}

void Chorus::updateHPFCoefficients(int channel) {
    if (channel < 0 || channel >= 2) return;
    
    // Calculate normalized frequency (0.0 to 1.0, where 1.0 = Nyquist)
    double normalizedFreq = config.hpfCutoff.load() / (sampleRate.load() * 0.5);
    normalizedFreq = std::clamp(normalizedFreq, 0.001, 0.99); // Prevent extreme values
    
    // Create HPF coefficients using JUCE's IIR filter design
    juce::IIRCoefficients coeffs = juce::IIRCoefficients::makeHighPass(sampleRate.load(), config.hpfCutoff.load(), 0.707f);
    
    // Apply coefficients to the HPF
    hpfFilters[channel].setCoefficients(coeffs);
}

void Chorus::processFilters(juce::AudioBuffer<float>& wetBuffer) {
    if (!prepared.load()) return;
    
    const int numSamples = wetBuffer.getNumSamples();
    const int numChannels = wetBuffer.getNumChannels();
    
    for (int channel = 0; channel < numChannels && channel < 2; ++channel) {
        float* channelData = wetBuffer.getWritePointer(channel);
        
        // Apply LPF if enabled
        if (config.lpfEnabled.load()) {
            for (int sample = 0; sample < numSamples; ++sample) {
                channelData[sample] = lpfFilters[channel].processSingleSampleRaw(channelData[sample]);
            }
        }
        
        // Apply HPF if enabled
        if (config.hpfEnabled.load()) {
            for (int sample = 0; sample < numSamples; ++sample) {
                channelData[sample] = hpfFilters[channel].processSingleSampleRaw(channelData[sample]);
            }
        }
    }
}

} // namespace WizardCore
