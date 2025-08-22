#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <atomic>

namespace audio_plugin {

/**
 * @brief A thread-safe, interpolating delay line for audio processing
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
 * DelayLine delay;
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
class DelayLine {
public:
    /**
     * @brief Interpolation types for fractional delay support
     */
    enum class InterpolationType {
        None,    ///< No interpolation (nearest sample)
        Linear   ///< Linear interpolation (recommended for most uses)
    };

    /**
     * @brief Default constructor
     */
    DelayLine();
    
    /**
     * @brief Destructor
     */
    virtual ~DelayLine();

    /**
     * @brief Prepare the delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    virtual void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels);

    /**
     * @brief Set the delay time for all channels
     * @param delayTimeInSeconds Delay time in seconds (will be clamped to max)
     * 
     * The delay time is clamped between 0 and the maximum delay time set in prepare().
     * When using Linear interpolation, this can be called at audio rate for smooth
     * modulation effects (chorus, flanger, etc.) without zipper noise.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    virtual void setDelayTime(double delayTimeInSeconds);

    /**
     * @brief Set the delay time with sample-accurate precision
     * @param delayInSamples Delay in samples (can be fractional)
     * 
     * Direct sample-based delay setting. With Linear interpolation enabled,
     * this supports smooth modulation at audio rate.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    virtual void setDelayInSamples(double delayInSamples);
    
    /**
     * @brief Set the delay time immediately without smoothing
     * @param delayTimeInSeconds Delay time in seconds
     * 
     * Bypasses smoothing for instant changes. Use with caution as this
     * may cause clicks if called during playback.
     */
    virtual void setDelayTimeImmediate(double delayTimeInSeconds);
    
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
    virtual void setSmoothingTime(double rampTimeInSeconds);

    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    virtual double getDelayTime() const;

    /**
     * @brief Get the current delay time in samples
     * @return Current delay time in samples (can be fractional)
     */
    virtual double getDelayInSamples() const;

    /**
     * @brief Get the current smoothed delay time in samples
     * @return Current smoothed delay time in samples
     */
    virtual double getCurrentDelayInSamples() const;

    /**
     * @brief Set the interpolation type for fractional delays
     * @param type Interpolation type to use
     */
    virtual void setInterpolationType(InterpolationType type);

    /**
     * @brief Get the current interpolation type
     * @return Current interpolation type
     */
    virtual InterpolationType getInterpolationType() const;

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
    virtual float processSample(int channel, float input);

    /**
     * @brief Process a block of audio
     * @param buffer Audio buffer to process in-place
     * 
     * Processes all channels in the buffer. This is more efficient than
     * calling processSample() for each sample individually.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual void processBlock(juce::AudioBuffer<float>& buffer);

    /**
     * @brief Clear all delay buffers
     * 
     * Resets all delay buffers to silence. Useful for stopping feedback
     * or clearing accumulated delay content.
     */
    virtual void clear();

    /**
     * @brief Check if the delay line is prepared for processing
     * @return true if prepared, false otherwise
     */
    virtual bool isPrepared() const;

    /**
     * @brief Get the maximum delay time in seconds
     * @return Maximum delay time in seconds
     */
    virtual double getMaxDelayTime() const;

    /**
     * @brief Get the maximum delay time in samples
     * @return Maximum delay time in samples
     */
    virtual int getMaxDelayInSamples() const;

    /**
     * @brief Get the current sample rate
     * @return Current sample rate in Hz
     */
    virtual double getSampleRate() const;

private:
    // Constants
    static constexpr double DEFAULT_SMOOTHING_TIME = 0.05;  // 50ms default
    static constexpr double MIN_DELAY_SAMPLES = 1.0;        // Minimum 1 sample delay
    
    // Member variables
    std::vector<std::vector<float>> buffers;        // Per-channel circular buffers
    std::vector<int> writeIndices;                  // Write position for each channel
    std::vector<double> readPositions;              // Read position for each channel
    std::atomic<bool> prepared{false};              // Preparation state
    double sampleRate{0.0};                         // Current sample rate
    int numChannels{0};                             // Number of audio channels
    double maxDelayInSamples{0.0};                  // Maximum delay in samples
    int bufferSize{0};                              // Size of each circular buffer
    
    // Smoothing and interpolation
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoothedDelay;
    double targetDelayInSamples{0.0};               // Target delay time
    InterpolationType interpolationType{InterpolationType::Linear};
    
    // Helper methods
    float processSampleLinearInterp(int channel, float input);
    float processSampleNoInterp(int channel, float input);
    float getInterpolatedSample(int channel, double readPosition);
    void updateReadPositions();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayLine)
};

} // namespace audio_plugin