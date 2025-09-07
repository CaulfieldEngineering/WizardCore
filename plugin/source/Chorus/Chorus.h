#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../LFO/LFO.h"
#include "../DelayLine/DelayLine.h"
#include <array>

namespace WizardCore {

/**
 * @brief A multi-voice chorus effect using modulated delay lines
 * 
 * This class provides a chorus effect with configurable number of modulated delay lines
 * mixed with the dry signal. Each voice has individual LFO objects that control
 * modulation parameters (rate, depth, phase offset) directly, plus mix and base delay.
 * 
 * Thread Safety: Thread-safe for audio processing
 * Memory: Allocates delay buffers and LFO wavetables during prepare()
 */
class Chorus {
public:
    // ============================================================================
    // ENUMS AND CONSTANTS
    // ============================================================================
    
    enum class PanningMode {
        Mono,           // Original mono behavior
        Stereo,         // Stereo phase offset mode
        MidSide         // Mid-Side processing mode
    };

    // ============================================================================
    // CONSTANTS
    // ============================================================================
    
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
    static constexpr float MIN_VOICE_ATTENUATION = -24.0f;  // Maximum attenuation per voice in dB
    static constexpr float MAX_VOICE_ATTENUATION = 0.0f;    // No attenuation per voice in dB
    static constexpr float MIN_MASTER_VOICE_LEVEL = -20.0f; // Maximum master voice attenuation in dB
    static constexpr float MAX_MASTER_VOICE_LEVEL = 6.0f;   // Maximum master voice boost in dB

    // ============================================================================
    // CONFIGURATION STRUCT
    // ============================================================================
    
    /**
     * @brief Sonic parameters that define the module's behavior and character
     * 
     * NOTE: This struct is for READ-ONLY ACCESS and parameter overview.
     * All modifications must use the dedicated setter methods.
     */
    struct Config {
        // Core sonic parameters
        std::atomic<bool> enabled{true};                    ///< Enable/disable entire chorus effect
        std::atomic<int> numActiveVoices{1};               ///< Number of currently active voices
        std::atomic<float> rate{1.0f};                     ///< Global LFO modulation rate in Hz
        std::atomic<float> depth{0.5f};                    ///< Global modulation depth [0.0, 1.0]
        std::atomic<float> mix{0.7f};                      ///< Global dry/wet mix [0.0, 1.0]
        std::atomic<float> baseDelay{30.0f};               ///< Global base delay time in milliseconds
        std::atomic<float> voiceAttenuationDb{0.0f};      ///< Voice attenuation per voice in dB (negative = quieter)
        std::atomic<float> masterVoiceLevelDb{-3.0f};      ///< Master voice level attenuation in dB (negative = quieter)
        
        // Stereo processing parameters
        std::atomic<PanningMode> panningMode{PanningMode::Mono}; ///< Current stereo processing mode
        std::atomic<float> stereoSpread{0.5f};             ///< Stereo spread amount [0.0, 1.0]
        std::atomic<bool> midEnabled{true};                ///< Enable mid channel processing
        std::atomic<bool> sideEnabled{true};               ///< Enable side channel processing
        std::atomic<float> sideGainDb{0.0f};              ///< Side channel gain in dB [-20, +20]
        
        // Filter parameters
        std::atomic<bool> lpfEnabled{false};               ///< Enable low-pass filtering
        std::atomic<bool> hpfEnabled{false};               ///< Enable high-pass filtering
        std::atomic<float> lpfCutoff{20000.0f};            ///< LPF cutoff frequency in Hz
        std::atomic<float> hpfCutoff{20.0f};               ///< HPF cutoff frequency in Hz
        
        // Delay type parameter
        std::atomic<DelayType> delayType{DelayType::BBDelay}; ///< Current delay line type
    };

    // ============================================================================
    // CONSTRUCTOR & DESTRUCTOR
    // ============================================================================
    
    /**
     * @brief Constructor with configurable maximum voices
     * @param maxVoices Maximum number of voices (1-16, default 5)
     */
    explicit Chorus(int maxVoices = 5);
    
    /**
     * @brief Destructor
     */
    ~Chorus();

    // ============================================================================
    // PUBLIC INTERFACE
    // ============================================================================
    
    /**
     * @brief Prepare the chorus for processing
     * @param sampleRate The sample rate in Hz
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    void prepare(double sampleRate, int numChannels);

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

    // ============================================================================
    // ACCESS TO SONIC CONFIGURATION (READ-ONLY)
    // ============================================================================
    
    /**
     * @brief Get access to sonic configuration for debugging/monitoring (READ-ONLY)
     * @return Const reference to the Config struct containing all sonic parameters
     */
    const Config& getConfig() const { return config; }

    // ============================================================================
    // INDIVIDUAL PARAMETER SETTERS
    // ============================================================================
    
    // Global parameter setters
    void setEnabled(bool enabled);
    void setNumVoices(int numVoices);
    void setRate(float rate);
    void setDepth(float depth);
    void setMix(float mix);
    void setBaseDelay(float delayMs);
    void setVoiceAttenuation(float attenuationDb);
    void setMasterVoiceLevel(float levelDb);
    void setStereoMode(PanningMode mode);
    void setStereoSpread(float spread);
    void setMidEnabled(bool enabled);
    void setSideEnabled(bool enabled);
    void setSideGain(float gainDb);
    void setLPFEnabled(bool enabled);
    void setHPFEnabled(bool enabled);
    void setLPFCutoff(float frequencyHz);
    void setHPFCutoff(float frequencyHz);
    void setDelayType(DelayType delayType);

    // Per-voice parameter setters
    void setVoiceEnabled(int voiceIndex, bool enabled);
    void setVoiceBaseDelay(int voiceIndex, float delayMs);

    // ============================================================================
    // INDIVIDUAL PARAMETER GETTERS
    // ============================================================================
    
    // Global parameter getters
    bool isEnabled() const { return config.enabled.load(); }
    int getNumVoices() const { return config.numActiveVoices.load(); }
    int getMaxVoices() const { return maxVoices; }
    float getRate() const { return config.rate.load(); }
    float getDepth() const { return config.depth.load(); }
    float getMix() const { return config.mix.load(); }
    float getBaseDelay() const { return config.baseDelay.load(); }
    float getVoiceAttenuation() const { return config.voiceAttenuationDb.load(); }
    float getMasterVoiceLevel() const { return config.masterVoiceLevelDb.load(); }
    PanningMode getStereoMode() const { return config.panningMode.load(); }
    float getStereoSpread() const { return config.stereoSpread.load(); }
    bool isMidEnabled() const { return config.midEnabled.load(); }
    bool isSideEnabled() const { return config.sideEnabled.load(); }
    float getSideGain() const { return config.sideGainDb.load(); }
    bool isLPFEnabled() const { return config.lpfEnabled.load(); }
    bool isHPFEnabled() const { return config.hpfEnabled.load(); }
    float getLPFCutoff() const { return config.lpfCutoff.load(); }
    float getHPFCutoff() const { return config.hpfCutoff.load(); }
    DelayType getDelayType() const { return config.delayType.load(); }
    double getSampleRate() const { return sampleRate.load(); }
    bool isPrepared() const { return prepared.load(); }

    // Per-voice parameter getters
    bool isVoiceEnabled(int voiceIndex) const;
    float getVoiceRate(int voiceIndex) const;
    float getVoiceDepth(int voiceIndex) const;
    float getVoiceBaseDelay(int voiceIndex) const;
    float getVoicePhaseOffset(int voiceIndex) const;

    // ============================================================================
    // LFO AND DELAY LINE ACCESS
    // ============================================================================
    
    /**
     * @brief Get direct access to a voice's LFO instance
     * @param voiceIndex Voice index (0 to maxVoices-1)
     * @return Pointer to the LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's left/mid LFO instance
     * @param voiceIndex Voice index (0 to maxVoices-1)
     * @return Pointer to the left/mid LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceLeftLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's right/side LFO instance
     * @param voiceIndex Voice index (0 to maxVoices-1)
     * @return Pointer to the right/side LFO instance, or nullptr if invalid index
     */
    LFO* getVoiceRightLFO(int voiceIndex);
    
    /**
     * @brief Get direct access to a voice's delay line instance
     * @param voiceIndex Voice index (0 to maxVoices-1)
     * @return Pointer to the DelayLine instance, or nullptr if invalid index
     */
    DelayLine* getVoiceDelayLine(int voiceIndex);

    // ============================================================================
    // BATCH PARAMETER UPDATES
    // ============================================================================
    
    /**
     * @brief Update all parameters efficiently in one call
     * @param rate LFO modulation rate in Hz
     * @param depth Modulation depth [0.0, 1.0]
     * @param mix Dry/wet mix [0.0, 1.0]
     * @param baseDelay Base delay time in milliseconds
     * @param voiceAttenuation Voice attenuation per voice in dB (negative = quieter)
     * @param masterVoiceLevel Master voice level attenuation in dB (negative = quieter)
     * @param voiceCount Number of active voices
     * @param panningMode Stereo processing mode
     * @param stereoSpread Stereo spread amount [0.0, 1.0]
     * @param midEnabled Enable mid channel processing
     * @param sideEnabled Enable side channel processing
     * @param sideGain Side channel gain in dB
     * @param lpfEnabled Enable low-pass filtering
     * @param lpfCutoff LPF cutoff frequency in Hz
     * @param hpfEnabled Enable high-pass filtering
     * @param hpfCutoff HPF cutoff frequency in Hz
     */
    void updateParameters(float rate, float depth, float mix, float baseDelay, float voiceAttenuation, float masterVoiceLevel, int voiceCount,
                         int panningMode, float stereoSpread,
                         bool midEnabled, bool sideEnabled, float sideGain,
                         bool lpfEnabled = false, float lpfCutoff = 20000.0f,
                         bool hpfEnabled = false, float hpfCutoff = 20.0f);

private:
    // ============================================================================
    // PRIVATE MEMBER VARIABLES
    // ============================================================================
    
    // Sonic parameters (what makes this module sound/behave differently)
    Config config;                                          // PRIVATE - internal storage
    
    // System/technical parameters (not sonic)
    std::atomic<bool> prepared{false};                     // PRIVATE - implementation state
    std::atomic<double> sampleRate{44100.0};               // PRIVATE - system value
    const int maxVoices;                                   // PRIVATE - construction parameter
    
    // ============================================================================
    // VOICE STRUCTURE
    // ============================================================================
    
    /**
     * @brief Individual voice structure containing LFOs and delay lines
     */
    struct Voice {
        // Dual LFO system for independent channel control
        std::array<LFO, 2> lfos;                           // [0] = left/mid, [1] = right/side
        std::array<std::unique_ptr<DelayLine>, 2> delayLines; // [0] = left/mid, [1] = right/side
        
        // Voice state parameters
        std::atomic<bool> enabled{false};                  ///< Voice enabled state
        std::atomic<float> baseDelayMs{30.0f};             ///< Voice-specific base delay time in milliseconds
    };
    
    // Core components - dynamic array of voices
    std::unique_ptr<Voice[]> voices;                       ///< Array of voice instances
    
    // ============================================================================
    // FILTER COMPONENTS
    // ============================================================================
    
    // JUCE filters for wet signal processing
    std::array<juce::IIRFilter, 2> lpfFilters;             // Left/Right or Mid/Side LPF
    std::array<juce::IIRFilter, 2> hpfFilters;             // Left/Right or Mid/Side HPF
    
    // ============================================================================
    // PRIVATE HELPER METHODS
    // ============================================================================
    
    // Voice management
    void initializeVoice(int voiceIndex);
    void syncRightLFOsToLeft();
    
    // Stereo processing methods
    void updateStereoConfiguration();
    void configureMonoPhaseOffsets();
    void configureStereoPhaseOffsets();
    void configureMidSidePhaseOffsets();
    void processVoicesMono(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesStereo(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    void processVoicesMidSide(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& wetBuffer);
    
    // Filter processing methods
    void processFilters(juce::AudioBuffer<float>& wetBuffer);
    void updateLPFCoefficients(int channel);
    void updateHPFCoefficients(int channel);
};

} // namespace WizardCore
