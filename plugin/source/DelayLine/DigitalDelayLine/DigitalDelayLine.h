#pragma once

#include "../DelayLine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <atomic>

namespace audio_plugin {

/**
 * @brief A thread-safe, interpolating digital delay line for audio processing
 * 
 * This class provides a circular buffer-based delay line with linear interpolation
 * for smooth delay time changes and modulation. It supports multi-channel processing
 * with independent delay buffers per channel.
 * 
 * Thread Safety: Thread-safe for parallel channel processing
 * Memory: Allocates (maxDelaySeconds * sampleRate * numChannels * sizeof(float)) bytes
 * 
 * Usage Example - Basic Delay:
 * @code
 * DigitalDelayLine delay;
 * delay.prepare(48000, 1.0, 2);  // 48kHz, 1 second max, stereo
 * delay.setDelayTime(0.25);       // 250ms delay
 * delay.processBlock(audioBuffer);
 * @endcode
 * 
 * Usage Example - Modulated Delay (Chorus/Flanger):
 * @code
 * // In processBlock:
 * float lfoValue = std::sin(lfoPhase);
 * float delayMs = 20.0f + (lfoValue * 10.0f);  // 20ms ± 10ms
 * delay.setDelayTime(delayMs * 0.001f);  // Convert to seconds
 * delay.processBlock(audioBuffer, 0.5f);  // 50% wet mix
 * @endcode
 */
class DigitalDelayLine : public DelayLine {
public:
    // ============================================================================
    // CONFIGURATION STRUCT
    // ============================================================================
    
    /**
     * @brief Configuration struct containing all delay line parameters of sonic/usable interest
     * 
     * This struct contains the parameters that define how the delay line sounds and behaves.
     * It is public so external code can access the parameter structure for debugging
     * and monitoring, but the actual config instance remains private.
     */
    struct Config {
        // Core sonic parameters
        std::atomic<double> delayTimeInSeconds{0.0};           ///< Delay time in seconds
        std::atomic<double> delayInSamples{0.0};              ///< Delay time in samples
        std::atomic<double> smoothingTimeInSeconds{0.05};      ///< Smoothing ramp time in seconds
        std::atomic<InterpolationType> interpolationType{InterpolationType::Linear}; ///< Interpolation method
        
        // Musical/sync parameters
        std::atomic<bool> enabled{true};                       ///< Enabled state
    };

    // ============================================================================
    // CONSTRUCTOR & DESTRUCTOR
    // ============================================================================

    /**
     * @brief Default constructor
     */
    DigitalDelayLine();
    
    /**
     * @brief Destructor
     */
    virtual ~DigitalDelayLine();

    // ============================================================================
    // PREPARATION & SETUP
    // ============================================================================
    
    /**
     * @brief Prepare the delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    virtual void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels) override;

    // ============================================================================
    // INDIVIDUAL PARAMETER SETTERS
    // ============================================================================
    
    /**
     * @brief Set the delay time for all channels
     * @param delayTimeInSeconds Delay time in seconds (will be clamped to max)
     * @param withSmoothing Whether to apply smoothing to prevent clicks (default: true)
     * 
     * The delay time is clamped between 0 and the maximum delay time set in prepare().
     * When using Linear interpolation, this can be called at audio rate for smooth
     * modulation effects (chorus, flanger, etc.) without zipper noise.
     * 
     * When withSmoothing is true, the delay time change is automatically smoothed.
     * When withSmoothing is false, the delay time is applied immediately (may cause clicks).
     */
    virtual void setDelayTimeInSeconds(double delayTimeInSeconds, bool withSmoothing = true) override;

    /**
     * @brief Set the delay time with sample-accurate precision
     * @param delayInSamples Delay in samples (can be fractional)
     * @param withSmoothing Whether to apply smoothing to prevent clicks (default: true)
     * 
     * Direct sample-based delay setting. With Linear interpolation enabled,
     * this supports smooth modulation at audio rate.
     * 
     * When withSmoothing is true, the delay time change is automatically smoothed.
     * When withSmoothing is false, the delay time is applied immediately (may cause clicks).
     */
    virtual void setDelayInSamples(double delayInSamples, bool withSmoothing = true) override;
    

    
    /**
     * @brief Set the smoothing ramp time for delay changes
     * @param rampTimeInSeconds Time in seconds for delay changes to ramp
     * 
     * Controls how quickly delay time changes are applied. Longer ramp times
     * prevent clicks but reduce modulation responsiveness. Shorter ramp times
     * allow faster modulation but may cause artifacts.
     * 
     * Default: 0.05 seconds (50ms) - good balance for most applications
     */
    virtual void setSmoothingTime(double rampTimeInSeconds) override;

    /**
     * @brief Set the interpolation type for fractional delays
     * @param type Interpolation type to use
     */
    virtual void setInterpolationType(InterpolationType type) override;

    /**
     * @brief Set the enabled state of the delay line
     * @param enabled Whether the delay line is enabled
     */
    void setEnabled(bool enabled);

    // ============================================================================
    // INDIVIDUAL PARAMETER GETTERS
    // ============================================================================
    
    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    virtual double getDelayTimeInSeconds() const override;

    /**
     * @brief Get the current delay time in samples
     * @return Current delay time in samples (can be fractional)
     */
    virtual double getDelayInSamples() const override;

    /**
     * @brief Get the current smoothed delay time in samples
     * @return Current smoothed delay time in samples
     */
    virtual double getCurrentDelayInSamples() const override;

    /**
     * @brief Get the current interpolation type
     * @return Current interpolation type
     */
    virtual InterpolationType getInterpolationType() const override;

    /**
     * @brief Get the current smoothing time in seconds
     * @return Current smoothing time in seconds
     */
    double getSmoothingTime() const;

    /**
     * @brief Check if the delay line is enabled
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const;

    /**
     * @brief Check if the delay line is prepared for processing
     * @return true if prepared, false otherwise
     */
    virtual bool isPrepared() const override;

    /**
     * @brief Get the maximum delay time in seconds
     * @return Maximum delay time in seconds
     */
    virtual double getMaxDelayTime() const override;

    /**
     * @brief Get the maximum delay time in samples
     * @return Maximum delay time in samples
     */
    virtual int getMaxDelayInSamples() const override;

    /**
     * @brief Get the current sample rate
     * @return Current sample rate in Hz
     */
    virtual double getSampleRate() const override;

    // ============================================================================
    // BATCH PARAMETER UPDATES
    // ============================================================================
    
    /**
     * @brief Update multiple parameters at once
     * @param delayTimeInSeconds Optional delay time in seconds
     * @param smoothingTimeInSeconds Optional smoothing time in seconds
     * @param interpolationType Optional interpolation type
     * @param enabled Optional enabled state
     */
    void updateParameters(std::optional<double> delayTimeInSeconds = std::nullopt,
                         std::optional<double> smoothingTimeInSeconds = std::nullopt,
                         std::optional<InterpolationType> interpolationType = std::nullopt,
                         std::optional<bool> enabled = std::nullopt);

    // ============================================================================
    // AUDIO PROCESSING
    // ============================================================================
    
    /**
     * @brief Process a single sample for a specific channel
     * @param channel Channel index (0-based)
     * @param input Input sample
     * @return Delayed output sample
     * 
     * This is the core processing method. It reads from the delay buffer
     * at the current delay time and writes the new input sample.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual float processSample(int channel, float input) override;

    /**
     * @brief Process a block of audio
     * @param buffer Audio buffer to process in-place
     * 
     * Processes all channels in the buffer. This is more efficient than
     * calling processSample() for each sample individually.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual void processBlock(juce::AudioBuffer<float>& buffer) override;
    
    // ============================================================================
    // UTILITY & MAINTENANCE
    // ============================================================================
    
    /**
     * @brief Clear all delay buffers
     * 
     * Resets all delay buffers to silence. Useful for stopping feedback
     * or clearing accumulated delay content.
     */
    virtual void clear() override;

    /**
     * @brief Get access to the sonic configuration for debugging/monitoring
     * @return Reference to the current sonic configuration
     */
    const Config& getConfig() const { return config; }

private:
    // ============================================================================
    // PRIVATE CONSTANTS
    // ============================================================================
    
    static constexpr double DEFAULT_SMOOTHING_TIME_SECONDS = 0.05;  ///< 50ms default smoothing
    static constexpr double MIN_DELAY_SAMPLES = 1.0;               ///< Minimum 1 sample delay
    
    // ============================================================================
    // PRIVATE MEMBER VARIABLES
    // ============================================================================
    
    // Sonic parameters (what makes this module sound/behave differently)
    Config config;                                             // PRIVATE - internal storage
    
    // System/technical parameters (not sonic)
    std::atomic<bool> prepared{false};                         // PRIVATE - implementation state
    std::atomic<double> sampleRateHz{44100.0};                // PRIVATE - system value
    std::atomic<int> numChannels{0};                          // PRIVATE - system value
    std::atomic<double> maxDelayInSamples{0.0};               // PRIVATE - system value
    std::atomic<int> bufferSize{0};                           // PRIVATE - system value
    
    // Processing state
    std::vector<std::vector<float>> buffers;                  ///< Per-channel circular buffers
    std::vector<int> writeIndices;                            ///< Write position for each channel
    std::vector<double> readPositions;                        ///< Read position for each channel
    
    // Smoothed parameters (prevent audio clicks)
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoothedDelay;
    std::atomic<double> targetDelayInSamples{0.0};            ///< Target delay time for smoothing
    
    // ============================================================================
    // PRIVATE HELPER METHODS
    // ============================================================================
    
    /**
     * @brief Process a single sample with linear interpolation
     * @param channel Channel index
     * @param input Input sample
     * @return Delayed output sample
     */
    float processSampleLinearInterp(int channel, float input);
    
    /**
     * @brief Process a single sample without interpolation
     * @param channel Channel index
     * @param input Input sample
     * @return Delayed output sample
     */
    float processSampleNoInterp(int channel, float input);
    
    /**
     * @brief Get interpolated sample from buffer at fractional position
     * @param channel Channel index
     * @param readPosition Fractional read position
     * @return Interpolated sample value
     */
    float getInterpolatedSample(int channel, double readPosition);
    
    /**
     * @brief Update read positions for all channels
     */
    void updateReadPositions();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DigitalDelayLine)
};

} // namespace audio_plugin
