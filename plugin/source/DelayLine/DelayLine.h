#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <atomic>

namespace audio_plugin {

/**
 * @brief Available delay line types
 */
enum class DelayType {
    DigitalDelay,    ///< Clean digital delay (DigitalDelayLine)
    BBDelay          ///< Bucket Brigade Delay emulation (BBDelayLine)
};

/**
 * @brief Abstract base interface for all delay line types
 * 
 * This abstract class defines the common interface that all delay line implementations
 * must provide. It enables polymorphic usage and seamless switching between different
 * delay types (digital, BBD, etc.) in effects like Chorus.
 * 
 * Thread Safety: All implementations must be thread-safe
 * Memory: Implementation-specific allocation
 * 
 * Usage Example - Polymorphic Delay:
 * @code
 * std::unique_ptr<DelayLine> delay = DelayLineFactory::createDelayLine(DelayType::DigitalDelay);
 * delay->prepare(48000, 1.0, 2);  // 48kHz, 1 second max, stereo
 * delay->setDelayTime(0.25);       // 250ms delay
 * delay->processBlock(audioBuffer);
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
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~DelayLine() = default;

    /**
     * @brief Prepare the delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     * 
     * Must be called before processing any audio. This allocates the internal
     * buffers and resets all state.
     */
    virtual void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels) = 0;

    /**
     * @brief Set the delay time for all channels
     * @param delayTimeInSeconds Delay time in seconds (will be clamped to max)
     * 
     * The delay time is clamped between 0 and the maximum delay time set in prepare().
     * When using Linear interpolation, this can be called at audio rate for smooth
     * modulation effects (chorus, flanger, etc.) without zipper noise.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    virtual void setDelayTime(double delayTimeInSeconds) = 0;

    /**
     * @brief Set the delay time with sample-accurate precision
     * @param delayInSamples Delay in samples (can be fractional)
     * 
     * Direct sample-based delay setting. With Linear interpolation enabled,
     * this supports smooth modulation at audio rate.
     * The delay time change is automatically smoothed to prevent clicks.
     */
    virtual void setDelayInSamples(double delayInSamples) = 0;
    
    /**
     * @brief Set the delay time immediately without smoothing
     * @param delayTimeInSeconds Delay time in seconds
     * 
     * Bypasses smoothing for instant changes. Use with caution as this
     * may cause clicks if called during playback.
     */
    virtual void setDelayTimeImmediate(double delayTimeInSeconds) = 0;
    
    /**
     * @brief Set the smoothing ramp time for delay changes
     * @param rampTimeInSeconds Time in seconds for delay changes to ramp
     * 
     * Controls how quickly delay time changes are smoothed. Longer ramp times
     * prevent clicks but may limit modulation speed. Shorter ramp times allow
     * faster modulation but may introduce artifacts.
     * 
     * Default: 0.01 seconds (10ms) - good balance for most uses
     */
    virtual void setSmoothingTime(double rampTimeInSeconds) = 0;
    
    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    virtual double getDelayTime() const = 0;
    
    /**
     * @brief Get the current delay time in samples
     * @return Current delay time in samples (can be fractional)
     */
    virtual double getDelayInSamples() const = 0;
    
    /**
     * @brief Get the current actual delay in samples (may differ from set value during smoothing)
     * @return Current actual delay in samples
     */
    virtual double getCurrentDelayInSamples() const = 0;
    
    /**
     * @brief Set the interpolation type for fractional delay support
     * @param type The interpolation type to use
     * 
     * Linear interpolation provides smoother delay changes and better quality
     * for fractional delays, but uses slightly more CPU. None is faster but
     * may produce artifacts during delay time changes.
     */
    virtual void setInterpolationType(InterpolationType type) = 0;
    
    /**
     * @brief Get the current interpolation type
     * @return Current interpolation type
     */
    virtual InterpolationType getInterpolationType() const = 0;
    
    /**
     * @brief Process a single sample through the delay line
     * @param channel The channel to process (0-based)
     * @param input The input sample
     * @return The delayed output sample
     * 
     * This is the core processing function. It reads from the delay buffer
     * at the current delay time and writes the input sample to the buffer.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual float processSample(int channel, float input) = 0;
    
    /**
     * @brief Process an entire audio block
     * @param buffer The audio buffer to process
     * 
     * Processes all samples in the buffer through the delay line.
     * This is more efficient than calling processSample() for each sample.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual void processBlock(juce::AudioBuffer<float>& buffer) = 0;
    
    /**
     * @brief Process an audio block with wet/dry mixing
     * @param buffer The audio buffer to process
     * @param wetMix The wet signal mix amount (0.0 = dry only, 1.0 = wet only)
     * 
     * Processes the buffer and mixes the delayed signal with the original.
     * Useful for effects where you want to blend the delayed and original signals.
     * 
     * Thread Safety: Safe to call from different threads for different channels
     */
    virtual void processBlock(juce::AudioBuffer<float>& buffer, float wetMix) = 0;
    
    /**
     * @brief Clear all delay buffers and reset state
     * 
     * Resets all internal buffers to zero and clears any accumulated state.
     * Useful for stopping feedback loops or resetting the effect.
     */
    virtual void clear() = 0;
    
    /**
     * @brief Check if the delay line is ready for processing
     * @return true if prepare() has been called, false otherwise
     */
    virtual bool isPrepared() const = 0;
    
    /**
     * @brief Get the maximum delay time in seconds
     * @return Maximum delay time in seconds
     */
    virtual double getMaxDelayTime() const = 0;
    
    /**
     * @brief Get the maximum delay time in samples
     * @return Maximum delay time in samples
     */
    virtual int getMaxDelayInSamples() const = 0;
    
    /**
     * @brief Get the current sample rate
     * @return Current sample rate in Hz
     */
    virtual double getSampleRate() const = 0;

    // Factory methods - static functions to create different delay line types
    /**
     * @brief Create a delay line instance of the specified type
     * @param type The type of delay line to create
     * @return Unique pointer to the created delay line
     */
    static std::unique_ptr<DelayLine> create(DelayType type = DelayType::BBDelay);
    
    /**
     * @brief Create a clean digital delay line
     * @return Unique pointer to a DigitalDelayLine instance
     */
    static std::unique_ptr<DelayLine> createDigital();
    
    /**
     * @brief Create a BBD delay line with vintage characteristics
     * @return Unique pointer to a BBDelayLine instance
     */
    static std::unique_ptr<DelayLine> createBBD();
    
    /**
     * @brief Create a BBD delay line with specified characteristics
     * @param characteristic The BBD characteristic to use (0=Vintage, 1=Modern, 2=Dirty)
     * @return Unique pointer to a BBDelayLine instance
     */
    static std::unique_ptr<DelayLine> createBBD(int characteristic);
};

} // namespace audio_plugin