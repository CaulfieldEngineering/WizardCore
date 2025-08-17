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
        
        // Initialize stereo phase offsets
        voices[i].leftPhaseOffset = 0.0f;
        voices[i].rightPhaseOffset = 0.0f;
    }
    
    // Initialize stereo configuration with default values
    currentStereoMode = StereoMode::Mono;
    stereoSpread = 0.5f;
    updateStereoConfiguration();
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
        
        // Prepare delay lines with maximum delay time needed
        // Base delay + max modulation depth = max possible delay
        double maxDelayTime = (voice.baseDelay.load() + MAX_DELAY_MS) * 0.001; // Convert ms to seconds
        
        // Prepare both delay lines independently
        for (int channel = 0; channel < 2; ++channel) {
            voice.delayLines[channel].prepare(sampleRateIn, maxDelayTime, 1);  // Mono delay line per channel
            voice.delayLines[channel].setInterpolationType(DelayLine::InterpolationType::Linear);
            voice.delayLines[channel].setDelayTime(voice.currentDelayTime);
            
            // Set longer smoothing time for low base delays to prevent warbling
            double smoothingTime = voice.baseDelay.load() < 45.0f ? 0.1 : 0.05; // 100ms vs 50ms
            voice.delayLines[channel].setSmoothingTime(smoothingTime);
        }
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
    
    //DBG("Chorus: Number of voices set to " << numVoices);
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

DelayLine* Chorus::getVoiceLeftDelayLine(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Invalid voice index for DelayLine access: " << voiceIndex);
        return nullptr;
    }
    return &voices[voiceIndex].delayLines[0];
}

DelayLine* Chorus::getVoiceRightDelayLine(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Invalid voice index for DelayLine access: " << voiceIndex);
        return nullptr;
    }
    return &voices[voiceIndex].delayLines[1];
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
        for (int channel = 0; channel < 2; ++channel) {
            voices[voiceIndex].delayLines[channel].setDelayTime(voices[voiceIndex].currentDelayTime);
        }
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
            for (int channel = 0; channel < 2; ++channel) {
                voices[i].delayLines[channel].setDelayTime(voices[i].currentDelayTime);
            }
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
    
    // Switch between mono and stereo processing based on current mode
    switch (currentStereoMode) {
        case StereoMode::Mono:
            processVoicesMono(buffer, wetBuffer);
            break;
        case StereoMode::Stereo:
            processVoicesStereo(buffer, wetBuffer);
            break;
        default:
            processVoicesMono(buffer, wetBuffer);  // Fallback to mono
            break;
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

void Chorus::processVoicesMono(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples <= 0 || numChannels <= 0) {
        return;
    }
    
    // Process each enabled voice
    for (int voiceIndex = 0; voiceIndex < MAX_VOICES; ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get LFO value for this sample
            float lfoValue = voice.lfo.getNextSample();
            
            // Calculate modulated delay time
            float modulationRange = voice.depth.load() * MAX_DELAY_MS * 0.001f; // Convert to seconds
            float modulatedDelay = voice.baseDelay.load() * 0.001f + (lfoValue - 0.5f) * 2.0f * modulationRange;
            
            // Clamp delay time to valid range
            modulatedDelay = std::max(0.001f, std::min(modulatedDelay, 0.1f)); // 1ms to 100ms
            
            // Get input samples for both channels
            const float* channelData[2] = { buffer.getReadPointer(0), buffer.getReadPointer(numChannels > 1 ? 1 : 0) };
            float* wetData[2] = { wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(numChannels > 1 ? 1 : 0) };
            
            // Process each channel with identical settings
            for (int channel = 0; channel < 2; ++channel) {
                voice.delayLines[channel].setDelayTime(modulatedDelay);
                float delayedSample = voice.delayLines[channel].processSample(0, channelData[channel][sample]);
                wetData[channel][sample] += delayedSample * voice.mix.load();
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
    for (int voiceIndex = 0; voiceIndex < MAX_VOICES; ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Get base delay and calculate modulation parameters
        float baseDelayMs = voice.baseDelay.load();
        float modulationRange = voice.depth.load() * MAX_DELAY_MS * 0.001f; // Convert to seconds
        
        // Adjust modulation range based on base delay to prevent artifacts
        if (baseDelayMs < 45.0f) {
            // Reduce modulation range for low base delays
            float scaleFactor = juce::jmap(baseDelayMs, 10.0f, 45.0f, 0.4f, 1.0f);
            modulationRange *= scaleFactor;
        }
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get base LFO value for this sample
            float baseLfoValue = voice.lfo.getNextSample();
            
            // Get input samples for both channels
            const float* channelData[2] = { buffer.getReadPointer(0), buffer.getReadPointer(1) };
            float* wetData[2] = { wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1) };
            
            // Process each channel
            for (int channel = 0; channel < 2; ++channel) {
                // Calculate phase-shifted LFO value for this channel
                float phaseOffset = (channel == 0) ? voice.leftPhaseOffset : voice.rightPhaseOffset;
                float phasedLfoValue = baseLfoValue + (phaseOffset / 360.0f);
                
                // Wrap to [0, 1] range
                phasedLfoValue = std::fmod(phasedLfoValue, 1.0f);
                if (phasedLfoValue < 0.0f) phasedLfoValue += 1.0f;
                
                // Calculate delay time for this channel
                float baseDelaySec = baseDelayMs * 0.001f;
                float modulation = (phasedLfoValue - 0.5f) * 2.0f * modulationRange;
                
                // Apply non-linear modulation scaling for smoother pitch variations
                modulation = std::copysign(std::pow(std::abs(modulation), 1.2f), modulation);
                
                float channelDelay = baseDelaySec + modulation;
                
                // Clamp delay time to valid range
                channelDelay = std::max(0.001f, std::min(channelDelay, 0.1f)); // 1ms to 100ms
                
                // Update delay time and process sample
                voice.delayLines[channel].setDelayTime(channelDelay);
                float delayedSample = voice.delayLines[channel].processSample(0, channelData[channel][sample]);
                
                // Add to wet buffer
                wetData[channel][sample] += delayedSample * voice.mix.load();
            }
        }
    }
}

void Chorus::clear() {
    if (prepared.load()) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            for (int channel = 0; channel < 2; ++channel) {
                voices[i].delayLines[channel].clear();
            }
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

void Chorus::setStereoMode(StereoMode mode) {
    currentStereoMode = mode;
    updateStereoConfiguration();
}

void Chorus::setStereoSpread(float spread) {
    stereoSpread = std::clamp(spread, 0.0f, 1.0f);
    updateStereoConfiguration();
}

void Chorus::updateStereoConfiguration() {
    if (currentStereoMode == StereoMode::Stereo) {
        // Use full 360° range for maximum stereo effect
        // Apply a non-linear curve to the spread parameter to make it more dramatic
        float spreadCurve = std::pow(stereoSpread, 0.5f); // Square root curve for more dramatic effect at lower values
        float maxPhaseOffset = spreadCurve * 360.0f;  // 0° to 360° max
        
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = voices[i];
            
            // At 0% spread: both channels get 0° offset (mono)
            // At 100% spread: left = -180°, right = +180° (maximum separation)
            voice.leftPhaseOffset = -maxPhaseOffset * 0.5f;
            voice.rightPhaseOffset = maxPhaseOffset * 0.5f;
            
            // Add slight offset per voice for richer stereo field
            float voiceSpread = (i * 15.0f) * spreadCurve; // Up to 15° additional spread per voice
            voice.leftPhaseOffset -= voiceSpread;
            voice.rightPhaseOffset += voiceSpread;
        }
    } else {
        // In mono mode, ensure no phase offsets
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = voices[i];
            voice.leftPhaseOffset = 0.0f;
            voice.rightPhaseOffset = 0.0f;
        }
    }
}


} // namespace audio_plugin
