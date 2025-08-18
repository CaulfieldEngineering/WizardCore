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
        
        // Initialize independent LFO parameters (start linked for backward compatibility)
        voices[i].lfoLinked = true;
        voices[i].leftRate = voices[i].rate.load();
        voices[i].rightRate = voices[i].rate.load();
        voices[i].leftDepth = voices[i].depth.load();
        voices[i].rightDepth = voices[i].depth.load();
        voices[i].leftPhaseOffset = 0.0f;
        voices[i].rightPhaseOffset = 0.0f;
        
        // Initialize mid-side phase offsets for backward compatibility
        voices[i].midPhaseOffset = 0.0f;
        voices[i].sidePhaseOffset = 0.0f;
    }
    
    // Initialize stereo configuration with default values
    currentStereoMode = StereoMode::Mono;
    stereoSpread = 0.5f;
    
    // Initialize mid-side configuration with default values
    midEnabled = true;
    sideEnabled = true;
    sideGainDb = 0.0f;
    
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
        
        // Prepare both LFOs with baked-in parameters for chorus effect
        for (int lfoIndex = 0; lfoIndex < 2; ++lfoIndex) {
            voice.lfos[lfoIndex].prepare(sampleRateIn);
            
            // Set LFO to sine wave with typical chorus settings
            voice.lfos[lfoIndex].setWaveShape(LFO::WaveformType::Sine);
            voice.lfos[lfoIndex].setInvert(false);
            voice.lfos[lfoIndex].setSymmetry(50.0f);  // 50% = symmetric
            voice.lfos[lfoIndex].setSyncToHost(false);
            voice.lfos[lfoIndex].setCoupling(LFO::CouplingType::AC);  // Use AC coupling for bipolar output [-1,1]
            
            // Set individual phase offsets - for linked mode, use voice phase offset
            if (voice.lfoLinked.load()) {
                voice.lfos[lfoIndex].setPhaseOffset(voice.phaseOffset.load());
            } else {
                // Use independent phase offsets
                float phaseOffset = (lfoIndex == 0) ? voice.leftPhaseOffset.load() : voice.rightPhaseOffset.load();
                voice.lfos[lfoIndex].setPhaseOffset(phaseOffset);
            }
        }
        
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
        return nullptr;
    }
    // Return left/mid LFO for backward compatibility
    return &voices[voiceIndex].lfos[0];
}

LFO* Chorus::getVoiceLeftLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return nullptr;
    }
    return &voices[voiceIndex].lfos[0];
}

LFO* Chorus::getVoiceRightLFO(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return nullptr;
    }
    return &voices[voiceIndex].lfos[1];
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

// Independent LFO getters
float Chorus::getVoiceLeftRate(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].leftRate.load();
}

float Chorus::getVoiceRightRate(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].rightRate.load();
}

float Chorus::getVoiceLeftDepth(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].leftDepth.load();
}

float Chorus::getVoiceRightDepth(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].rightDepth.load();
}

float Chorus::getVoiceLeftPhaseOffset(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].leftPhaseOffset.load();
}

float Chorus::getVoiceRightPhaseOffset(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].rightPhaseOffset.load();
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
    
    // Update LFO frequency if already prepared
    if (prepared.load()) {
        if (voices[voiceIndex].lfoLinked.load()) {
            voices[voiceIndex].lfos[0].setFrequency(static_cast<double>(rateInHz));
            voices[voiceIndex].lfos[1].setFrequency(static_cast<double>(rateInHz));
            
            // Sync independent parameters when linked
            voices[voiceIndex].leftRate = rateInHz;
            voices[voiceIndex].rightRate = rateInHz;
        }
    }
    
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
    
    // Sync independent depth parameters when linked
    if (voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].leftDepth = depthIn;
        voices[voiceIndex].rightDepth = depthIn;
    }
    
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
    
    // Clamp phase offset to valid range
    phaseOffset = juce::jlimit(MIN_PHASE_OFFSET, MAX_PHASE_OFFSET, phaseOffset);
    voices[voiceIndex].phaseOffset = phaseOffset;
    
    // Update both LFOs if linked
    if (voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].lfos[0].setPhaseOffset(phaseOffset);
        voices[voiceIndex].lfos[1].setPhaseOffset(phaseOffset);
    }
}

// Independent LFO control methods
void Chorus::setVoiceLFOLinked(int voiceIndex, bool linked) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    voices[voiceIndex].lfoLinked = linked;
    
    if (linked) {
        // When linking, sync independent parameters to global voice parameters
        voices[voiceIndex].leftRate = voices[voiceIndex].rate.load();
        voices[voiceIndex].rightRate = voices[voiceIndex].rate.load();
        voices[voiceIndex].leftDepth = voices[voiceIndex].depth.load();
        voices[voiceIndex].rightDepth = voices[voiceIndex].depth.load();
        voices[voiceIndex].leftPhaseOffset = voices[voiceIndex].phaseOffset.load();
        voices[voiceIndex].rightPhaseOffset = voices[voiceIndex].phaseOffset.load();
        
        // Update LFO parameters
        voices[voiceIndex].lfos[0].setPhaseOffset(voices[voiceIndex].phaseOffset.load());
        voices[voiceIndex].lfos[1].setPhaseOffset(voices[voiceIndex].phaseOffset.load());
    }
}

bool Chorus::isVoiceLFOLinked(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return true; // Default to linked for invalid index
    }
    return voices[voiceIndex].lfoLinked.load();
}

void Chorus::setVoiceLeftRate(int voiceIndex, float rateInHz) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    rateInHz = juce::jlimit(MIN_RATE, MAX_RATE, rateInHz);
    voices[voiceIndex].leftRate = rateInHz;
    
    if (!voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].lfos[0].setFrequency(rateInHz);
    }
}

void Chorus::setVoiceRightRate(int voiceIndex, float rateInHz) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    rateInHz = juce::jlimit(MIN_RATE, MAX_RATE, rateInHz);
    voices[voiceIndex].rightRate = rateInHz;
    
    if (!voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].lfos[1].setFrequency(rateInHz);
    }
}

void Chorus::setVoiceLeftDepth(int voiceIndex, float depth) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    depth = juce::jlimit(MIN_DEPTH, MAX_DEPTH, depth);
    voices[voiceIndex].leftDepth = depth;
}

void Chorus::setVoiceRightDepth(int voiceIndex, float depth) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    depth = juce::jlimit(MIN_DEPTH, MAX_DEPTH, depth);
    voices[voiceIndex].rightDepth = depth;
}

void Chorus::setVoiceLeftPhaseOffset(int voiceIndex, float phaseOffset) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    phaseOffset = juce::jlimit(MIN_PHASE_OFFSET, MAX_PHASE_OFFSET, phaseOffset);
    voices[voiceIndex].leftPhaseOffset = phaseOffset;
    
    if (!voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].lfos[0].setPhaseOffset(phaseOffset);
    }
}

void Chorus::setVoiceRightPhaseOffset(int voiceIndex, float phaseOffset) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        DBG("Chorus: Voice index out of range: " << voiceIndex);
        return;
    }
    
    phaseOffset = juce::jlimit(MIN_PHASE_OFFSET, MAX_PHASE_OFFSET, phaseOffset);
    voices[voiceIndex].rightPhaseOffset = phaseOffset;
    
    if (!voices[voiceIndex].lfoLinked.load()) {
        voices[voiceIndex].lfos[1].setPhaseOffset(phaseOffset);
    }
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
        
        // Update both LFOs if linked, otherwise only update linked parameters
        if (voices[i].lfoLinked.load()) {
            voices[i].lfos[0].setFrequency(static_cast<double>(rateInHz));
            voices[i].lfos[1].setFrequency(static_cast<double>(rateInHz));
            
            // Sync independent parameters when linked
            voices[i].leftRate = rateInHz;
            voices[i].rightRate = rateInHz;
        }
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
        
        // Sync independent depth parameters when linked
        if (voices[i].lfoLinked.load()) {
            voices[i].leftDepth = depthIn;
            voices[i].rightDepth = depthIn;
        }
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
    
    // Switch between mono, stereo, and mid-side processing based on current mode
    switch (currentStereoMode) {
        case StereoMode::Mono:
            processVoicesMono(buffer, wetBuffer);
            break;
        case StereoMode::Stereo:
            processVoicesStereo(buffer, wetBuffer);
            break;
        case StereoMode::MidSide:
            processVoicesMidSide(buffer, wetBuffer);
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
            // Get LFO value for this sample - use left/mid LFO for mono processing
            float lfoValue;
            if (voice.lfoLinked.load()) {
                // Use left LFO with global voice parameters
                lfoValue = voice.lfos[0].getNextSample();
            } else {
                // Use independent left LFO parameters
                lfoValue = voice.lfos[0].getNextSample();
            }
            
            // Calculate modulated delay time (simplified with AC coupling)
            float modulationRange;
            if (voice.lfoLinked.load()) {
                modulationRange = voice.depth.load() * MAX_DELAY_MS * 0.001f; // Convert to seconds
            } else {
                // Use average of left and right depths for mono mode
                float avgDepth = (voice.leftDepth.load() + voice.rightDepth.load()) * 0.5f;
                modulationRange = avgDepth * MAX_DELAY_MS * 0.001f;
            }
            
            float modulatedDelay = voice.baseDelay.load() * 0.001f + lfoValue * modulationRange;
            
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
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get input samples for both channels
            const float* channelData[2] = { buffer.getReadPointer(0), buffer.getReadPointer(1) };
            float* wetData[2] = { wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1) };
            
            // Get LFO values for this sample
            float lfoValues[2];
            float depths[2];
            
            if (voice.lfoLinked.load()) {
                // Use shared LFO with phase offsets for stereo effect
                float baseLfoValue = voice.lfos[0].getNextSample();
                
                for (int channel = 0; channel < 2; ++channel) {
                    float phaseOffset = (channel == 0) ? voice.leftPhaseOffset : voice.rightPhaseOffset;
                    lfoValues[channel] = baseLfoValue + (phaseOffset / 360.0f);
                    
                    // Wrap to appropriate range (assuming AC coupling gives [-1, 1])
                    lfoValues[channel] = std::fmod(lfoValues[channel], 2.0f);
                    if (lfoValues[channel] > 1.0f) lfoValues[channel] -= 2.0f;
                    if (lfoValues[channel] < -1.0f) lfoValues[channel] += 2.0f;
                    
                    depths[channel] = voice.depth.load();
                }
            } else {
                // Use independent LFOs for each channel
                lfoValues[0] = voice.lfos[0].getNextSample();
                lfoValues[1] = voice.lfos[1].getNextSample();
                depths[0] = voice.leftDepth.load();
                depths[1] = voice.rightDepth.load();
            }
            
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
                voice.delayLines[channel].setDelayTime(channelDelay);
                float delayedSample = voice.delayLines[channel].processSample(0, channelData[channel][sample]);
                
                // Add to wet buffer
                wetData[channel][sample] += delayedSample * voice.mix.load();
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
    for (int voiceIndex = 0; voiceIndex < MAX_VOICES; ++voiceIndex) {
        Voice& voice = voices[voiceIndex];
        
        if (!voice.enabled.load()) {
            continue;
        }
        
        // Get base delay and calculate modulation parameters
        float baseDelayMs = voice.baseDelay.load();
        
        // Process each sample for this voice
        for (int sample = 0; sample < numSamples; ++sample) {
            // Get LFO values for this sample (one per LFO)
            float midLfoValue, sideLfoValue;
            float midDepth, sideDepth;
            
            if (voice.lfoLinked.load()) {
                // Use single LFO with phase offsets for backward compatibility
                float baseLfoValue = voice.lfos[0].getNextSample();
                
                // Calculate mid channel LFO value
                float midPhaseOffset = voice.midPhaseOffset;
                midLfoValue = baseLfoValue + (midPhaseOffset / 360.0f);
                
                // Wrap to appropriate range (assuming AC coupling gives [-1, 1])
                midLfoValue = std::fmod(midLfoValue, 2.0f);
                if (midLfoValue > 1.0f) midLfoValue -= 2.0f;
                if (midLfoValue < -1.0f) midLfoValue += 2.0f;
                
                // Calculate side channel LFO value
                float sidePhaseOffset = voice.sidePhaseOffset;
                sideLfoValue = baseLfoValue + (sidePhaseOffset / 360.0f);
                
                // Wrap to appropriate range
                sideLfoValue = std::fmod(sideLfoValue, 2.0f);
                if (sideLfoValue > 1.0f) sideLfoValue -= 2.0f;
                if (sideLfoValue < -1.0f) sideLfoValue += 2.0f;
                
                midDepth = voice.depth.load();
                sideDepth = voice.depth.load();
            } else {
                // Use independent LFOs
                midLfoValue = voice.lfos[0].getNextSample();  // Mid/left LFO
                sideLfoValue = voice.lfos[1].getNextSample(); // Side/right LFO
                midDepth = voice.leftDepth.load();
                sideDepth = voice.rightDepth.load();
            }
            
            // Process mid channel (channel 0) if enabled
            if (midEnabled) {
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
                voice.delayLines[0].setDelayTime(midDelay);
                float delayedSample = voice.delayLines[0].processSample(0, midSideData[0][sample]);
                
                // Add to wet buffer
                midSideWetData[0][sample] += delayedSample * voice.mix.load();
            }
            
            // Process side channel (channel 1) if enabled
            if (sideEnabled) {
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
                voice.delayLines[1].setDelayTime(sideDelay);
                float delayedSample = voice.delayLines[1].processSample(0, midSideData[1][sample]);
                
                // Add to wet buffer with configurable side gain
                // Convert dB to linear gain: gain = 10^(dB/20)
                float sideGainLinear = std::pow(10.0f, sideGainDb / 20.0f);
                midSideWetData[1][sample] += delayedSample * voice.mix.load() * sideGainLinear;
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
        float finalMid = midEnabled ? midSideWetData[0][sample] : dryMid;
        float finalSide = sideEnabled ? midSideWetData[1][sample] : drySide;
        
        // M/S decoding: L = (M + S)/2, R = (M - S)/2
        float wetLeft = (finalMid + finalSide) * 0.5f;
        float wetRight = (finalMid - finalSide) * 0.5f;
        
        wetBuffer.addSample(0, sample, wetLeft);
        wetBuffer.addSample(1, sample, wetRight);
    }
}

void Chorus::clear() {
    if (prepared.load()) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            for (int channel = 0; channel < 2; ++channel) {
                voices[i].delayLines[channel].clear();
            }
            // Reset both LFOs
            voices[i].lfos[0].reset();
            voices[i].lfos[1].reset();
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

void Chorus::setMidEnabled(bool enabled) {
    midEnabled = enabled;
}

void Chorus::setSideEnabled(bool enabled) {
    sideEnabled = enabled;
}

void Chorus::setSideGain(float gainDb) {
    sideGainDb = std::clamp(gainDb, -20.0f, 20.0f);
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
            
            // Clear mid-side offsets in stereo mode
            voice.midPhaseOffset = 0.0f;
            voice.sidePhaseOffset = 0.0f;
        }
    } else if (currentStereoMode == StereoMode::MidSide) {
        // Use full 360° range for maximum mid-side effect
        // Apply a non-linear curve to the spread parameter to make it more dramatic
        float spreadCurve = std::pow(stereoSpread, 0.5f); // Square root curve for more dramatic effect at lower values
        float maxPhaseOffset = spreadCurve * 360.0f;  // 0° to 360° max
        
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = voices[i];
            
            // At 0% spread: both mid and side get 0° offset (mono)
            // At 100% spread: mid = -180°, side = +180° (maximum separation)
            voice.midPhaseOffset = -maxPhaseOffset * 0.5f;
            voice.sidePhaseOffset = maxPhaseOffset * 0.5f;
            
            // Add slight offset per voice for richer mid-side field
            float voiceSpread = (i * 15.0f) * spreadCurve; // Up to 15° additional spread per voice
            voice.midPhaseOffset -= voiceSpread;
            voice.sidePhaseOffset += voiceSpread;
            
            // Clear stereo offsets in mid-side mode
            voice.leftPhaseOffset = 0.0f;
            voice.rightPhaseOffset = 0.0f;
        }
    } else {
        // In mono mode, ensure no phase offsets
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = voices[i];
            voice.leftPhaseOffset = 0.0f;
            voice.rightPhaseOffset = 0.0f;
            voice.midPhaseOffset = 0.0f;
            voice.sidePhaseOffset = 0.0f;
        }
    }
}


} // namespace audio_plugin
