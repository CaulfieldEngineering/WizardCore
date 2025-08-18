#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../LFO/LFO.h"
#include "../DelayLine/DelayLine.h"
#include <array>

namespace audio_plugin {

/**
 * @brief A multi-voice chorus effect using modulated delay lines
 * 
 * This class provides a chorus effect with up to configurable number of modulated delay lines
 * mixed with the dry signal. Each voice has individual parameters for
 * rate, depth, mix, base delay, and phase offset.
 * 
 * Thread Safety: Thread-safe for audio processing
 * Memory: Allocates delay buffers and LFO wavetables during prepare()
 * 
 * Usage Example:
 * @code
 * Chorus chorus(3);  // Create chorus with 3 voices maximum
 * chorus.prepare(48000, 2);  // 48kHz, stereo
 * chorus.setVoiceEnabled(0, true);  // Enable first voice
 * chorus.setVoiceRate(0, 1.0);      // 1 Hz modulation for voice 0
 * chorus.setVoiceDepth(0, 0.5);     // 50% modulation depth for voice 0
 * chorus.setVoiceMix(0, 0.7);       // 70% wet, 30% dry for voice 0
 * chorus.setVoiceBaseDelay(0, 30.0); // 30ms base delay for voice 0
 * chorus.setVoicePhaseOffset(0, 0.0); // No phase offset for voice 0
 * chorus.processBlock(audioBuffer);
 * @endcode
 */
class Chorus {
public:
    /**
     * @brief Constructor with configurable maximum voices
     * @param maxVoices Maximum number of voices (1-16, default 5)
     */
    explicit Chorus(int maxVoices = 5);
    
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
    /**
     * @brief Set the LFO modulation rate for a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param rateInHz Modulation rate in Hz (0.1 to 2.0 Hz typical for chorus)
     */
    void setVoiceRate(int voiceIndex, float rateInHz);

    /**
     * @brief Set the modulation depth for a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param depth Modulation depth (0.0 to 1.0)
     */
    void setVoiceDepth(int voiceIndex, float depth);

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

    /**
     * @brief Set the phase offset for a specific voice
     * @param voiceIndex Voice index (0-4)
     * @param phaseOffset Phase offset in degrees (0-360)
     */
    void setVoicePhaseOffset(int voiceIndex, float phaseOffset);

    // Independent LFO control methods
    /**
     * @brief Enable or disable LFO linking for a voice
     * @param voiceIndex Voice index (0-4)
     * @param linked When true, both LFOs use global voice parameters; when false, use independent parameters
     */
    void setVoiceLFOLinked(int voiceIndex, bool linked);
    
    /**
     * @brief Check if a voice's LFOs are linked
     * @param voiceIndex Voice index (0-4)
     * @return True if LFOs are linked (using global voice parameters)
     */
    bool isVoiceLFOLinked(int voiceIndex) const;
    
    /**
     * @brief Set independent rate for left/mid LFO
     * @param voiceIndex Voice index (0-4)
     * @param rateInHz Modulation rate in Hz for left/mid channel
     */
    void setVoiceLeftRate(int voiceIndex, float rateInHz);
    
    /**
     * @brief Set independent rate for right/side LFO
     * @param voiceIndex Voice index (0-4)
     * @param rateInHz Modulation rate in Hz for right/side channel
     */
    void setVoiceRightRate(int voiceIndex, float rateInHz);
    
    /**
     * @brief Set independent depth for left/mid LFO
     * @param voiceIndex Voice index (0-4)
     * @param depth Modulation depth for left/mid channel (0.0 to 1.0)
     */
    void setVoiceLeftDepth(int voiceIndex, float depth);
    
    /**
     * @brief Set independent depth for right/side LFO
     * @param voiceIndex Voice index (0-4)
     * @param depth Modulation depth for right/side channel (0.0 to 1.0)
     */
    void setVoiceRightDepth(int voiceIndex, float depth);
    
    /**
     * @brief Set independent phase offset for left/mid LFO
     * @param voiceIndex Voice index (0-4)
     * @param phaseOffset Phase offset in degrees for left/mid channel (0-360)
     */
    void setVoiceLeftPhaseOffset(int voiceIndex, float phaseOffset);
    
    /**
     * @brief Set independent phase offset for right/side LFO
     * @param voiceIndex Voice index (0-4)
     * @param phaseOffset Phase offset in degrees for right/side channel (0-360)
     */
    void setVoiceRightPhaseOffset(int voiceIndex, float phaseOffset);

    // Global parameter setters (affect all voices)
    /**
     * @brief Set the LFO modulation rate for all voices
     * @param rateInHz Modulation rate in Hz (0.1 to 2.0 Hz typical for chorus)
     */
    void setRate(float rateInHz);

    /**
     * @brief Set the modulation depth for all voices
     * @param depth Modulation depth (0.0 to 1.0)
     */
    void setDepth(float depth);

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

    // Getters (all thread-safe)
    float getRate() const { return rate.load(); }
    float getDepth() const { return depth.load(); }
    float getMix() const { return mix.load(); }
    float getBaseDelay() const { return baseDelay.load(); }
    double getSampleRate() const { return sampleRate.load(); }
    
    // Per-voice getters
    float getVoiceRate(int voiceIndex) const;
    float getVoiceDepth(int voiceIndex) const;
    float getVoiceMix(int voiceIndex) const;
    float getVoiceBaseDelay(int voiceIndex) const;
    float getVoicePhaseOffset(int voiceIndex) const;
    DelayLine* getVoiceLeftDelayLine(int voiceIndex);
    DelayLine* getVoiceRightDelayLine(int voiceIndex);
    
    // Independent LFO getters
    float getVoiceLeftRate(int voiceIndex) const;
    float getVoiceRightRate(int voiceIndex) const;
    float getVoiceLeftDepth(int voiceIndex) const;
    float getVoiceRightDepth(int voiceIndex) const;
    float getVoiceLeftPhaseOffset(int voiceIndex) const;
    float getVoiceRightPhaseOffset(int voiceIndex) const;

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

private:
    // Internal helper methods
    void updateLFO();
    void updateDelayTime();
    
    // Voice structure
    struct Voice {
        // Dual LFO system for independent channel control
        std::array<LFO, 2> lfos;  // [0] = left/mid, [1] = right/side
        std::array<DelayLine, 2> delayLines;  // [0] = left/mid, [1] = right/side
        
        std::atomic<bool> enabled{false};
        
        // Global voice parameters (affect both LFOs when linked)
        std::atomic<float> rate{1.0f};
        std::atomic<float> depth{0.5f};
        std::atomic<float> mix{0.7f};
        std::atomic<float> baseDelay{30.0f};
        std::atomic<float> phaseOffset{0.0f};
        
        // Independent LFO parameters for advanced control
        std::atomic<float> leftRate{1.0f};      // Independent rate for left/mid LFO
        std::atomic<float> rightRate{1.0f};     // Independent rate for right/side LFO
        std::atomic<float> leftDepth{0.5f};     // Independent depth for left/mid LFO
        std::atomic<float> rightDepth{0.5f};    // Independent depth for right/side LFO
        std::atomic<float> leftPhaseOffset{0.0f};   // Independent phase for left/mid LFO
        std::atomic<float> rightPhaseOffset{0.0f};  // Independent phase for right/side LFO
        
        // LFO linking control
        std::atomic<bool> lfoLinked{true};      // When true, both LFOs use global parameters
        
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
    std::atomic<float> rate{1.0f};
    std::atomic<float> depth{0.5f};
    std::atomic<float> mix{0.7f};
    std::atomic<float> baseDelay{30.0f};
    std::atomic<double> sampleRate{44100.0};
    std::atomic<bool> prepared{false};
    std::atomic<int> numActiveVoices{1};
    std::atomic<bool> enabled{true};
    
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
    
    // Stereo processing methods
    void processVoicesMono(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesStereo(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesMidSide(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void updateStereoConfiguration();
};

} // namespace audio_plugin
