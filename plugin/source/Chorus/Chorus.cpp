#include "Chorus.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace audio_plugin {

Chorus::Chorus() {
    // Initialize with default values for global parameters
    rate = 1.0f;
    depth = 0.5f;
    mix = 0.5f;
    baseDelay = 30.0f;
    sampleRate = 44100.0;
    prepared = false;
    
    // Initialize all voices with default values
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].enabled = (i == 0); // Only first voice enabled by default
        voices[i].rate = 0.8f + (i * 0.15f); // Slightly different rates for each voice (0.8, 0.95, 1.1, 1.25, 1.4)
        voices[i].depth = 0.5f;
        voices[i].mix = 0.5f - (i * 0.1f); // Decreasing mix for each voice (0.5, 0.4, 0.3, 0.2, 0.1)
        voices[i].baseDelay = 30.0f + (i * 3.0f); // Slightly different delays for each voice (30, 33, 36, 39, 42)
        voices[i].phaseOffset = i * 72.0f; // 72 degrees apart (360/5)
        voices[i].currentLfoValue = 0.0f;
        voices[i].currentDelayTime = voices[i].baseDelay.load() * 0.001f;
    }
}

Chorus::~Chorus() {
    // Destructor - no cleanup needed as RAII handles everything
}

void Chorus::prepare(double sampleRateIn, int numChannels) {
    if (sampleRateIn <= 0.0 || numChannels <= 0) {
        DBG("Chorus: Invalid parameters in prepare()");
        return;
    }
    
    sampleRate = sampleRateIn;
    
    // Prepare each voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        Voice& voice = voices[i];
        
        // Prepare LFO with baked-in parameters for chorus effect
        voice.lfo.prepare(sampleRateIn);
        
        // Set LFO to sine wave with typical chorus settings
        voice.lfo.setWaveShape(LFO::WaveformType::Sine);
        voice.lfo.setInvert(false);
        voice.lfo.setSymmetry(50.0f);  // 50% = symmetric
        voice.lfo.setPhaseOffset(voice.phaseOffset.load()); // Individual phase offset
        voice.lfo.setSyncToHost(false);
        
        // Prepare delay line with maximum delay time needed
        // Base delay + max modulation depth = max possible delay
        double maxDelayTime = (voice.baseDelay.load() + MAX_DELAY_MS) * 0.001; // Convert ms to seconds
        voice.delayLine.prepare(sampleRateIn, maxDelayTime, numChannels);
        
        // Set delay line to linear interpolation for smooth modulation
        voice.delayLine.setInterpolationType(DelayLine::InterpolationType::Linear);
        
        // Set initial delay time
        voice.currentDelayTime = voice.baseDelay.load() * 0.001f;
        voice.delayLine.setDelayTime(voice.currentDelayTime);
    }
    
    prepared = true;
    
    // DBG("Chorus: Prepared successfully with sample rate: " << sampleRateIn 
    //     << ", channels: " << numChannels << ", voices: " << MAX_VOICES);
}

// Voice management methods
void Chorus::setVoiceEnabled(int voiceIndex, bool enabled) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    voices[voiceIndex].enabled = enabled;
    
    // DBG("Chorus: Voice " << voiceIndex << " " << (enabled ? "enabled" : "disabled"));
}

bool Chorus::isVoiceEnabled(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return false;
    }
    return voices[voiceIndex].enabled.load();
}

void Chorus::setNumVoices(int numVoices) {
    if (numVoices < 1 || numVoices > MAX_VOICES) {
        DBG("Chorus: Invalid number of voices: " << numVoices << " (must be 1-" << MAX_VOICES << ")");
        return;
    }
    
    // Enable the first numVoices voices, disable the rest
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].enabled = (i < numVoices);
    }
    
    numActiveVoices = numVoices;
    
    DBG("Chorus: Number of voices set to " << numVoices);
}

int Chorus::getNumVoices() const {
    return numActiveVoices.load();
}

LFO* Chorus::getVoiceLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Invalid voice index for LFO access: " << voiceIndex);
        return nullptr;
    }
    return &voices[voiceIndex].lfo;
}

DelayLine* Chorus::getVoiceDelayLine(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Invalid voice index for DelayLine access: " << voiceIndex);
        return nullptr;
    }
    return &voices[voiceIndex].delayLine;
}

// Per-voice parameter setters
void Chorus::setVoiceRate(int voiceIndex, float rateInHz) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (rateInHz < MIN_RATE || rateInHz > MAX_RATE) {
        DBG("Chorus: Rate out of range: " << rateInHz << " Hz");
        return;
    }
    
    voices[voiceIndex].rate = rateInHz;
    voices[voiceIndex].lfo.setFrequency(static_cast<double>(rateInHz));
    
    // DBG("Chorus: Voice " << voiceIndex << " rate set to " << rateInHz << " Hz");
}

void Chorus::setVoiceDepth(int voiceIndex, float depthIn) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (depthIn < MIN_DEPTH || depthIn > MAX_DEPTH) {
        DBG("Chorus: Depth out of range: " << depthIn);
        return;
    }
    
    voices[voiceIndex].depth = depthIn;
    
    // DBG("Chorus: Voice " << voiceIndex << " depth set to " << depthIn);
}

void Chorus::setVoiceMix(int voiceIndex, float mixIn) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (mixIn < MIN_MIX || mixIn > MAX_MIX) {
        DBG("Chorus: Mix out of range: " << mixIn);
        return;
    }
    
    voices[voiceIndex].mix = mixIn;
    
    // DBG("Chorus: Voice " << voiceIndex << " mix set to " << mixIn);
}

void Chorus::setVoiceBaseDelay(int voiceIndex, float delayMs) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (delayMs < MIN_BASE_DELAY || delayMs > MAX_BASE_DELAY) {
        DBG("Chorus: Base delay out of range: " << delayMs << " ms");
        return;
    }
    
    voices[voiceIndex].baseDelay = delayMs;
    
    // Update current delay time if already prepared
    if (prepared.load()) {
        voices[voiceIndex].currentDelayTime = delayMs * 0.001f;
        voices[voiceIndex].delayLine.setDelayTime(voices[voiceIndex].currentDelayTime);
    }
    
    // DBG("Chorus: Voice " << voiceIndex << " base delay set to " << delayMs << " ms");
}

void Chorus::setVoicePhaseOffset(int voiceIndex, float phaseOffset) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    if (phaseOffset < MIN_PHASE_OFFSET || phaseOffset > MAX_PHASE_OFFSET) {
        DBG("Chorus: Phase offset out of range: " << phaseOffset << " degrees");
        return;
    }
    
    voices[voiceIndex].phaseOffset = phaseOffset;
    
    // Update LFO phase offset if already prepared
    if (prepared.load()) {
        voices[voiceIndex].lfo.setPhaseOffset(phaseOffset);
    }
    
    // DBG("Chorus: Voice " << voiceIndex << " phase offset set to " << phaseOffset << " degrees");
}

// Global parameter setters (affect all voices)
void Chorus::setRate(float rateInHz) {
    if (rateInHz < MIN_RATE || rateInHz > MAX_RATE) {
        DBG("Chorus: Rate out of range: " << rateInHz << " Hz");
        return;
    }
    
    rate = rateInHz;
    
    // Update voices based on current count, not enabled state
    int currentVoiceCount = numActiveVoices.load();
    for (int i = 0; i < currentVoiceCount; ++i) {
        voices[i].rate = rateInHz;
        voices[i].lfo.setFrequency(static_cast<double>(rateInHz));
    }
    
    // DBG("Chorus: Global rate set to " << rateInHz << " Hz");
}

void Chorus::setDepth(float depthIn) {
    if (depthIn < MIN_DEPTH || depthIn > MAX_DEPTH) {
        DBG("Chorus: Depth out of range: " << depthIn);
        return;
    }
    
    depth = depthIn;
    
    // Update voices based on current count, not enabled state
    int currentVoiceCount = numActiveVoices.load();
    for (int i = 0; i < currentVoiceCount; ++i) {
        voices[i].depth = depthIn;
    }
    
    // DBG("Chorus: Global depth set to " << depthIn);
}

void Chorus::setMix(float mixIn) {
    if (mixIn < MIN_MIX || mixIn > MAX_MIX) {
        DBG("Chorus: Mix out of range: " << mixIn);
        return;
    }
    
    mix = mixIn;
    
    // Update voices based on current count, not enabled state
    int currentVoiceCount = numActiveVoices.load();
    for (int i = 0; i < currentVoiceCount; ++i) {
        voices[i].mix = mixIn;
    }
    
    // DBG("Chorus: Global mix set to " << mixIn);
}

void Chorus::setBaseDelay(float delayMs) {
    if (delayMs < MIN_BASE_DELAY || delayMs > MAX_BASE_DELAY) {
        DBG("Chorus: Base delay out of range: " << delayMs << " ms");
        return;
    }
    
    baseDelay = delayMs;
    
    // Update voices based on current count, not enabled state
    int currentVoiceCount = numActiveVoices.load();
    for (int i = 0; i < currentVoiceCount; ++i) {
        voices[i].baseDelay = delayMs;
        if (prepared.load()) {
            voices[i].currentDelayTime = delayMs * 0.001f;
            voices[i].delayLine.setDelayTime(voices[i].currentDelayTime);
        }
    }
    
    // DBG("Chorus: Global base delay set to " << delayMs << " ms");
}

void Chorus::setEnabled(bool enabledIn) {
    enabled = enabledIn;
    
    // DBG("Chorus: " << (enabledIn ? "enabled" : "disabled"));
}

void Chorus::processBlock(juce::AudioBuffer<float>& buffer) {
    if (!prepared.load()) {
        DBG("Chorus: Not prepared, skipping processing");
        return;
    }
    
    if (!enabled.load()) {
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
    
    // Process each enabled voice
    for (int voiceIndex = 0; voiceIndex < MAX_VOICES; ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get LFO value for this sample
            voice.currentLfoValue = voice.lfo.getNextSample();
            
            // Calculate modulated delay time
            // LFO output is [0,1], we want [-depth, +depth] range around base delay
            float modulationRange = voice.depth.load() * MAX_DELAY_MS * 0.001f; // Convert to seconds
            float modulatedDelay = voice.baseDelay.load() * 0.001f + (voice.currentLfoValue - 0.5f) * 2.0f * modulationRange;
            
            // Clamp delay time to valid range
            modulatedDelay = std::max(0.001f, std::min(modulatedDelay, 0.1f)); // 1ms to 100ms
            
            // Update delay line if delay time changed significantly
            if (std::abs(modulatedDelay - voice.currentDelayTime) > 0.0001f) {
                voice.currentDelayTime = modulatedDelay;
                voice.delayLine.setDelayTime(voice.currentDelayTime);
            }
            
            // Process each channel
            for (int channel = 0; channel < numChannels; ++channel) {
                const float* channelData = buffer.getReadPointer(channel);
                float* wetChannelData = wetBuffer.getWritePointer(channel);
                float inputSample = channelData[sample];
                
                // Get delayed sample
                float delayedSample = voice.delayLine.processSample(channel, inputSample);
                
                // Add to wet buffer (will be mixed later)
                // Normalize by the number of active voices to prevent volume buildup
                wetChannelData[sample] += delayedSample * voice.mix.load();
            }
        }
    }
    
    // Normalize wet buffer by number of active voices to prevent volume buildup
    int activeVoiceCount = 0;
    for (int i = 0; i < MAX_VOICES; ++i) {
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
    
    // Mix dry and wet signals
    for (int channel = 0; channel < numChannels; ++channel) {
        float* channelData = buffer.getWritePointer(channel);
        const float* wetChannelData = wetBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample) {
            float mixValue = mix.load();
            
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
        for (int i = 0; i < MAX_VOICES; ++i) {
            voices[i].delayLine.clear();
            voices[i].lfo.reset();
            voices[i].currentLfoValue = 0.0f;
            voices[i].currentDelayTime = voices[i].baseDelay.load() * 0.001f;
        }
        
        // DBG("Chorus: Cleared all buffers");
    }
}

bool Chorus::isPrepared() const {
    return prepared.load();
}

// Per-voice getters
float Chorus::getVoiceRate(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].rate.load();
}

float Chorus::getVoiceDepth(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].depth.load();
}

float Chorus::getVoiceMix(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].mix.load();
}

float Chorus::getVoiceBaseDelay(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].baseDelay.load();
}

float Chorus::getVoicePhaseOffset(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].phaseOffset.load();
}

void Chorus::updateLFO() {
    // This method is called internally to update LFO parameters
    // Currently not needed as parameters are set directly
}

void Chorus::updateDelayTime() {
    // This method is called internally to update delay time
    // Currently not needed as delay time is updated in processBlock
}

} // namespace audio_plugin
