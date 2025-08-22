#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../LFO/LFO.h"
//#include "../DelayLine/DelayLine.h"
#include "../DelayLineFactory/DelayLineFactory.h"
#include <array>

namespace audio_plugin {

/**
 * @brief A multi-voice chorus effect using modulated delay lines
 * 
 * This class provides a chorus effect with up to configurable number of modulated delay lines
 * mixed with the dry signal. Each voice has individual LFO objects that control
 * modulation parameters (rate, depth, phase offset) directly, plus mix and base delay.
 * 
 * Thread Safety: Thread-safe for audio processing
 * Memory: Allocates delay buffers and LFO wavetables during prepare()
 * 
 * Usage Example:
 * @code
 * Chorus chorus(3);  // Create chorus with 3 voices maximum
 * chorus.prepare(48000, 2);  // 48kHz, stereo
 * chorus.setVoiceEnabled(0, true);  // Enable first voice
 * 
 * // LFO parameters are now controlled directly via LFO objects:
 * chorus.getVoiceLeftLFO(0)->setFrequency(1.0);     // 1 Hz modulation for voice 0
 * chorus.getVoiceLeftLFO(0)->setDepth(0.5f);        // 50% modulation depth for voice 0
 * chorus.getVoiceLeftLFO(0)->setPhaseOffset(0.0);   // No phase offset for voice 0
 * 
 * chorus.setVoiceMix(0, 0.7);       // 70% wet, 30% dry for voice 0
 * chorus.setVoiceBaseDelay(0, 30.0); // 30ms base delay for voice 0
 * 
 * // Switch between different delay types
 * chorus.setDelayType(DelayType::BBDelay);     // Use BBD delay for vintage character
 * chorus.setDelayType(DelayType::DigitalDelay); // Use clean delay for pristine sound
 * 
 * chorus.processBlock(audioBuffer);
 * @endcode
 */
class Chorus {
public:
    /**
     * @brief Constructor with configurable maximum voices
     * @param maxVoices Maximum number of voices (1-16, default 5)
     */
    explicit Chorus(int maxVoices = 10);
    
    /**
     * @brief Destructor
     */
    ~Chorus();

    /**
     * @brief Prepare the chorus for processing
     * @param sampleRate The sample rate in Hz
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    void prepare(double sampleRate, int numChannels);

    // Voice management
    /**
     * @brief Enable or disable a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param enabled Whether the voice should be enabled
     */
    void setVoiceEnabled(int voiceIndex, bool enabled);
    
    /**
     * @brief Check if a voice is enabled
     * @param voiceIndex Voice index (0-4)
     * @return True if the voice is enabled
     */
    bool isVoiceEnabled(int voiceIndex) const;
    
    /**
     * @brief Set the number of active voices
     * @param numVoices Number of voices to enable (1-5)
     */
    void setNumVoices(int numVoices);
    
    /**
     * @brief Get the number of active voices
     * @return Number of currently enabled voices
     */
    int getNumVoices() const;
    
    /**
     * @brief Get the maximum number of voices supported by this instance
     * @return Maximum number of voices
     */
    int getMaxVoices() const;
    
    /**
     * @brief Get direct access to a voice's LFO instance
     * @param voiceIndex Voice index (0-4)
     * @return Pointer to the LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's left/mid LFO instance
     * @param voiceIndex Voice index (0-4)
     * @return Pointer to the left/mid LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceLeftLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's right/side LFO instance
     * @param voiceIndex Voice index (0-4)
     * @return Pointer to the right/side LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceRightLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's delay line instance
     * @param voiceIndex Voice index (0-4)
     * @return Pointer to the DelayLine instance, or nullptr if invalid index
     */
    DelayLine* getVoiceDelayLine(int voiceIndex);

    // Per-voice parameter setters
    // Note: LFO parameters (rate, depth, phase) are now controlled directly via LFO objects
    // Use: chorus.getVoiceLeftLFO(index)->setFrequency(), setDepth(), setPhaseOffset(), etc.

    /**
     * @brief Set the dry/wet mix for a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param mix Mix between dry (0.0) and wet (1.0) signal
     */
    void setVoiceMix(int voiceIndex, float mix);

    /**
     * @brief Set the base delay time for a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param delayMs Base delay time in milliseconds (20-40ms typical for chorus)
     */
    void setVoiceBaseDelay(int voiceIndex, float delayMs);

    // Note: Phase offset now controlled directly via LFO object
    // Use: chorus.getVoiceLeftLFO(index)->setPhaseOffset(radians)

    // Independent LFO control methods removed - LFOs are now always independent
    // Access LFO parameters directly via:
    // - getVoiceLeftLFO(index)->setFrequency(), setDepth(), setPhaseOffset()
    // - getVoiceRightLFO(index)->setFrequency(), setDepth(), setPhaseOffset()

    // Global parameter setters (affect all voices)
    // Note: LFO parameters (rate, depth) are now controlled directly via individual LFO objects
    // To set all voices: for(int i=0; i<maxVoices; ++i) getVoiceLeftLFO(i)->setFrequency(rate);

    /**
     * @brief Set the dry/wet mix for all voices
     * @param mix Mix between dry (0.0) and wet (1.0) signal
     */
    void setMix(float mix);

    /**
     * @brief Set the base delay time for all voices
     * @param delayMs Base delay time in milliseconds (20-40ms typical for chorus)
     */
    void setBaseDelay(float delayMs);
    
    /**
     * @brief Enable or disable the entire chorus effect
     * @param enabled Whether the chorus should be enabled
     */
    void setEnabled(bool enabled);

    /**
     * @brief Process a block of audio
     * @param buffer Audio buffer to process in-place
     * 
     * Processes all channels in the buffer. The chorused signal from all
     * enabled voices is mixed with the original signal.
     */
    void processBlock(juce::AudioBuffer<float>& buffer);

    /**
     * @brief Clear all internal buffers
     * 
     * Resets all delay buffers and LFO state to zero.
     */
    void clear();

    /**
     * @brief Check if the chorus is prepared and ready to process
     * @return True if prepare() has been called and the chorus is ready
     */
    bool isPrepared() const;
    
    /**
     * @brief Switch between different delay types for all voices
     * @param delayType The type of delay to use (DigitalDelay or BBDelay)
     */
    void setDelayType(DelayType delayType);

    // Getters (all thread-safe)
    // Note: getRate() and getDepth() removed - access via individual LFO objects
    // Use: getVoiceLeftLFO(index)->getFrequency() and getVoiceLeftLFO(index)->getDepth()
    float getMix() const { return mix.load(); }
    float getBaseDelay() const { return baseDelay.load(); }
    double getSampleRate() const { return sampleRate.load(); }
    bool isEnabled() const { return enabled.load(); }
    DelayType getDelayType() const { return currentDelayType.load(); }
    
    // Per-voice getters
    float getVoiceRate(int voiceIndex) const;
    float getVoiceDepth(int voiceIndex) const;
    float getVoiceMix(int voiceIndex) const;
    float getVoiceBaseDelay(int voiceIndex) const;
    float getVoicePhaseOffset(int voiceIndex) const;
    DelayLine* getVoiceLeftDelayLine(int voiceIndex);
    DelayLine* getVoiceRightDelayLine(int voiceIndex);
    
    // Independent LFO getters removed - access directly via LFO objects:
    // getVoiceLeftLFO(index)->getFrequency(), getDepth(), getPhaseOffset(), etc.

    enum class StereoMode {
        Mono,           // Original mono behavior
        Stereo,         // Stereo phase offset mode
        MidSide         // Mid-Side processing mode
    };
    
    // Stereo control methods
    void setStereoMode(StereoMode mode);
    void setStereoSpread(float spread);  // 0.0 = mono, 1.0 = maximum stereo
    StereoMode getStereoMode() const { return currentStereoMode; }
    float getStereoSpread() const { return stereoSpread; }
    
    // Mid-Side control methods
    void setMidEnabled(bool enabled);
    void setSideEnabled(bool enabled);
    void setSideGain(float gainDb);  // -20dB to +20dB
    bool isMidEnabled() const { return midEnabled; }
    bool isSideEnabled() const { return sideEnabled; }
    float getSideGain() const { return sideGainDb; }

    // Filter control methods
    void setLPFEnabled(bool enabled);
    void setHPFEnabled(bool enabled);
    void setLPFCutoff(float frequencyHz);
    void setHPFCutoff(float frequencyHz);
    bool isLPFEnabled() const { return lpfEnabled; }
    bool isHPFEnabled() const { return hpfEnabled; }
    float getLPFCutoff() const { return lpfCutoff; }
    float getHPFCutoff() const { return hpfCutoff; }

    // Individual parameter setters
    void setRate(float rate);
    void setDepth(float depth);
    void setVoiceCount(int voiceCount);
    void setStereoMode(int stereoMode);
    
    // Bulk parameter update method for efficient parameter changes
    void updateParameters(float rate, float depth, float mix, float baseDelay, int voiceCount,
                         int stereoMode, float stereoSpread,
                         bool midEnabled, bool sideEnabled, float sideGain,
                         bool lpfEnabled = false, float lpfCutoff = 20000.0f,
                         bool hpfEnabled = false, float hpfCutoff = 20.0f);

private:
    // Internal helper methods
    void syncRightLFOsToLeft();
    
    // Voice structure
    struct Voice {
        // Dual LFO system for independent channel control
        std::array<LFO, 2> lfos;  // [0] = left/mid, [1] = right/side
        std::array<std::unique_ptr<DelayLine>, 2> delayLines;  // [0] = left/mid, [1] = right/side - polymorphic for different delay types
        
        std::atomic<bool> enabled{false};
        
        // Non-LFO voice parameters (chorus-specific, not duplicated in LFO)
        std::atomic<float> mix{0.7f};
        std::atomic<float> baseDelay{30.0f};
        
        // Note: All LFO parameters (rate, depth, phase) are now stored ONLY in the LFO objects
        // Access via: lfos[0].getFrequency(), lfos[0].getDepth(), lfos[0].getPhaseOffset(), etc.
        
        // Current state variables
        float currentLfoValue{0.0f};
        float currentDelayTime{0.03f};
        
        // Mid-Side channel offsets (for backward compatibility)
        float midPhaseOffset{0.0f};
        float sidePhaseOffset{0.0f};
    };
    
    // Core components - dynamic array of voices
    std::unique_ptr<Voice[]> voices;
    const int maxVoices;  // Maximum number of voices (set at construction)
    
    // Global parameters (thread-safe using atomics)
    // Note: LFO parameters (rate, depth, phase) are stored in individual LFO objects
    std::atomic<float> mix{0.7f};
    std::atomic<float> baseDelay{30.0f};
    std::atomic<double> sampleRate{44100.0};
    std::atomic<bool> prepared{false};
    std::atomic<int> numActiveVoices{1};
    std::atomic<bool> enabled{true};
    std::atomic<DelayType> currentDelayType{DelayType::BBDelay};  // Track current delay type - default to BBD
    
    // Constants
    static constexpr int DEFAULT_MAX_VOICES = 5;
    static constexpr int MIN_MAX_VOICES = 1;
    static constexpr int MAX_MAX_VOICES = 16;
    static constexpr float MIN_RATE = 0.1f;
    static constexpr float MAX_RATE = 2.0f;
    static constexpr float MIN_DEPTH = 0.0f;
    static constexpr float MAX_DEPTH = 1.0f;
    static constexpr float MIN_MIX = 0.0f;
    static constexpr float MAX_MIX = 1.0f;
    static constexpr float MIN_BASE_DELAY = 10.0f;
    static constexpr float MAX_BASE_DELAY = 100.0f;
    static constexpr float MIN_DELAY_MS = 1.0f;
    static constexpr float MAX_DELAY_MS = 10.0f;
    static constexpr float MIN_PHASE_OFFSET = 0.0f;
    static constexpr float MAX_PHASE_OFFSET = 360.0f;

    // Stereo parameters
    StereoMode currentStereoMode = StereoMode::Mono;
    float stereoSpread = 0.5f;
    
    // Mid-Side parameters
    bool midEnabled = true;
    bool sideEnabled = true;
    float sideGainDb = 0.0f;  // Side gain in dB (-20 to +20)
    
    // Filter parameters
    bool lpfEnabled = false;
    bool hpfEnabled = false;
    float lpfCutoff = 20000.0f;  // Hz
    float hpfCutoff = 20.0f;     // Hz
    
    // JUCE filters for wet signal processing
    std::array<juce::IIRFilter, 2> lpfFilters;  // Left/Right or Mid/Side
    std::array<juce::IIRFilter, 2> hpfFilters;  // Left/Right or Mid/Side
    
    // Stereo processing methods
    void processVoicesMono(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesStereo(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesMidSide(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void updateStereoConfiguration();
    void configureMonoPhaseOffsets();
    void configureStereoPhaseOffsets();
    void configureMidSidePhaseOffsets();
    
    // Filter processing method
    void processFilters(juce::AudioBuffer<float>& wetBuffer);
    
    // Internal filter coefficient update methods
    void updateLPFCoefficients(int channel);
    void updateHPFCoefficients(int channel);
    
    // Voice configuration helper methods
    float calculateVoiceMixRatio(int voiceIndex) const;
};

} // namespace audio_plugin
