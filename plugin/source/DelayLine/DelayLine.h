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
 * float delayMs = 20.0f + (lfoValue * 10.0f);  // 20ms � 10ms
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
    ~DelayLine();

    /**
     * @brief Prepare the delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels);

    /**
     * @brief Set the delay time for all channels
     * @param delayTimeInSeconds Delay time in seconds (will be clamped to max)
     * 
     * The delay time is clamped between 0 and the maximum delay time set in prepare().
     * When using Linear interpolation, this can be called at audio rate for smooth
     * modulation effects (chorus, flanger, etc.) without zipper noise.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    void setDelayTime(double delayTimeInSeconds);

    /**
     * @brief Set the delay time with sample-accurate precision
     * @param delayInSamples Delay in samples (can be fractional)
     * 
     * Direct sample-based delay setting. With Linear interpolation enabled,
     * this supports smooth modulation at audio rate.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    void setDelayInSamples(double delayInSamples);
    
    /**
     * @brief Set the delay time immediately without smoothing
     * @param delayTimeInSeconds Delay time in seconds
     * 
     * Bypasses smoothing for instant changes. Use with caution as this
     * may cause clicks if called during playback.
     */
    void setDelayTimeImmediate(double delayTimeInSeconds);
    
    /**
     * @brief Set the smoothing ramp time for delay changes
     * @param rampTimeInSeconds Time in seconds for delay changes to ramp
     * 
     * Default is 0.05 seconds (50ms). Set to 0 to disable smoothing.
     */
    void setSmoothingTime(double rampTimeInSeconds);

    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    double getDelayTime() const;

    /**
     * @brief Get the current delay in samples
     * @return Current delay in samples (may be fractional)
     */
    double getDelayInSamples() const;

    /**
     * @brief Set the interpolation type
     * @param type The interpolation type to use
     */
    void setInterpolationType(InterpolationType type);

    /**
     * @brief Get the current interpolation type
     * @return Current interpolation type
     */
    InterpolationType getInterpolationType() const;

    /**
     * @brief Process a single sample for a specific channel
     * @param channel Channel index (0-based)
     * @param input Input sample
     * @return Delayed output sample
     * 
     * Note: prepare() must be called before processing
     */
    float processSample(int channel, float input);

    /**
     * @brief Process a block of audio
     * @param buffer Audio buffer to process in-place
     * 
     * Processes all channels in the buffer. The delayed signal replaces
     * the input signal in the buffer.
     */
    void processBlock(juce::AudioBuffer<float>& buffer);



    /**
     * @brief Clear all delay buffers
     * 
     * Resets all internal buffers to zero and resets write positions.
     */
    void clear();

    /**
     * @brief Check if the delay line is prepared and ready to process
     * @return True if prepare() has been called and the delay is ready
     */
    bool isPrepared() const;

    /**
     * @brief Get the maximum delay time in seconds
     * @return Maximum delay time that was set in prepare()
     */
    double getMaxDelayTime() const;

    /**
     * @brief Get the maximum delay in samples
     * @return Maximum delay in samples based on sample rate and max delay time
     */
    int getMaxDelayInSamples() const;

    /**
     * @brief Get the current sample rate
     * @return Sample rate in Hz
     */
    double getSampleRate() const;

private:
    // Internal helper methods
    float processSampleLinearInterp(int channel, float input);
    float processSampleNoInterp(int channel, float input);
    double getCurrentDelayInSamples() const;
    
    // Per-channel circular buffers and write indices
    std::vector<std::vector<float>> buffers;
    std::vector<int> writeIndices;
    
    // Delay parameters with smoothing
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoothedDelay;
    double targetDelayInSamples{0.0};
    double sampleRate{44100.0};
    double maxDelayInSamples{0.0};
    int bufferSize{0};
    int numChannels{0};
    
    // State
    std::atomic<bool> prepared{false};
    InterpolationType interpolationType{InterpolationType::Linear};
    
    // Constants
    static constexpr double MIN_DELAY_SAMPLES = 0.0;
    static constexpr double DEFAULT_SMOOTHING_TIME = 0.05; // 50ms default
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayLine)
};

} // namespace audio_plugin