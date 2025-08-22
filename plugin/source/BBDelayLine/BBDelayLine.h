#pragma once

#include "../DelayLine/DelayLine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

namespace audio_plugin {

/**
 * @brief A Bucket Brigade Delay emulation that inherits from DelayLine
 * 
 * This class emulates the characteristic sound of analog Bucket Brigade Delay (BBD) circuits,
 * which were commonly used in vintage delay pedals and synthesizers. It inherits from DelayLine
 * to maintain full interface compatibility while adding BBD-specific characteristics.
 * 
 * BBD Characteristics:
 * - Clock noise and jitter simulation
 * - Bandwidth reduction and filtering
 * - Slight sample rate conversion artifacts
 * - Warm, slightly degraded sound quality
 * - Clock rate modulation effects
 * 
 * Thread Safety: Inherits thread safety from DelayLine
 * Memory: Minimal additional overhead beyond base DelayLine
 * 
 * Usage Example:
 * @code
 * BBDelayLine bbDelay;
 * bbDelay.prepare(48000, 1.0, 2);  // 48kHz, 1 second max, stereo
 * bbDelay.setDelayTime(0.25);       // 250ms delay
 * bbDelay.setClockRate(1000);       // 1kHz internal clock
 * bbDelay.setNoiseAmount(0.1);      // 10% clock noise
 * bbDelay.processBlock(audioBuffer);
 * @endcode
 */
class BBDelayLine : public DelayLine {
public:
    /**
     * @brief BBD-specific parameters for emulating analog characteristics
     */
    enum class BBDCharacteristic {
        Vintage,    ///< Classic BBD sound with more noise and filtering
        Modern,     ///< Cleaner BBD with subtle artifacts
        Dirty       ///< Heavy degradation and noise for lo-fi effects
    };

    /**
     * @brief Default constructor
     */
    BBDelayLine();
    
    /**
     * @brief Destructor
     */
    ~BBDelayLine() override = default;

    /**
     * @brief Prepare the BBD delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     * 
     * Overrides DelayLine::prepare() to initialize BBD-specific parameters.
     * Must be called before processing any audio.
     */
    void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels) override;

    /**
     * @brief Set the internal clock rate for the BBD simulation
     * @param clockRateHz Clock rate in Hz (typically 1000-10000 Hz)
     * 
     * The clock rate determines the internal sample rate of the BBD circuit.
     * Lower rates create more artifacts, higher rates are cleaner.
     * Typical range: 1kHz to 10kHz for vintage character.
     */
    void setClockRate(double clockRateHz);

    /**
     * @brief Get the current clock rate
     * @return Current clock rate in Hz
     */
    double getClockRate() const;

    /**
     * @brief Set the amount of clock noise/jitter
     * @param noiseAmount Amount of noise (0.0 to 1.0, where 0.0 is clean)
     * 
     * Controls the amount of clock jitter and noise artifacts.
     * 0.0 = clean digital delay, 1.0 = maximum vintage BBD character.
     */
    void setNoiseAmount(double noiseAmount);

    /**
     * @brief Get the current noise amount
     * @return Current noise amount (0.0 to 1.0)
     */
    double getNoiseAmount() const;

    /**
     * @brief Set the bandwidth reduction amount
     * @param bandwidthReduction Amount of high-frequency reduction (0.0 to 1.0)
     * 
     * Simulates the bandwidth limitations of BBD circuits.
     * 0.0 = full bandwidth, 1.0 = maximum filtering.
     */
    void setBandwidthReduction(double bandwidthReduction);

    /**
     * @brief Get the current bandwidth reduction
     * @return Current bandwidth reduction amount (0.0 to 1.0)
     */
    double getBandwidthReduction() const;

    /**
     * @brief Set the BBD characteristic type
     * @param characteristic The BBD characteristic type
     * 
     * Preset configurations for different BBD circuit types.
     */
    void setBBDCharacteristic(BBDCharacteristic characteristic);

    /**
     * @brief Get the current BBD characteristic
     * @return Current BBD characteristic type
     */
    BBDCharacteristic getBBDCharacteristic() const;

    /**
     * @brief Process a single audio sample with BBD characteristics
     * @param channel Audio channel to process
     * @param input Input sample value
     * @return Processed sample with BBD effects applied
     * 
     * Overrides DelayLine::processSample() to add BBD processing.
     * The input sample is first processed through BBD simulation, then
     * passed to the base delay line processing.
     */
    float processSample(int channel, float input) override;

    /**
     * @brief Process a block of audio with BBD characteristics
     * @param buffer Audio buffer to process in-place
     * 
     * Overrides DelayLine::processBlock() to add BBD processing.
     * Each sample is processed through BBD simulation before being
     * passed to the base delay line processing.
     */
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    /**
     * @brief Clear all delay buffers and reset BBD state
     * 
     * Overrides DelayLine::clear() to also clear BBD-specific state.
     * This resets all delay buffers, modulation state, and filter coefficients.
     */
    void clear() override;

    /**
     * @brief Set clock rate modulation for chorus/flanger effects
     * @param modulationDepth Depth of modulation (0.0 to 1.0)
     * @param modulationRate Rate of modulation in Hz
     * 
     * Adds subtle clock rate modulation for more authentic BBD behavior.
     */
    void setClockModulation(double modulationDepth, double modulationRate);

    /**
     * @brief Get the current modulation parameters
     * @param depth Output parameter for modulation depth
     * @param rate Output parameter for modulation rate
     */
    void getClockModulation(double& depth, double& rate) const;

private:
    // BBD-specific processing methods
    float applyBBDProcessing(int channel, float input);
    void updateClockModulation();
    float applyBandwidthFilter(int channel, float sample);
    void updateFilterCoefficient();
    
    // BBD parameters
    double clockRate{2000.0};           // Internal clock rate in Hz
    double noiseAmount{0.15};           // Amount of clock noise (0.0 to 1.0)
    double bandwidthReduction{0.3};     // High-frequency reduction (0.0 to 1.0)
    BBDCharacteristic characteristic{BBDCharacteristic::Vintage};
    
    // Clock modulation
    double modulationDepth{0.0};
    double modulationRate{0.0};
    double modulationPhase{0.0};
    
    // Internal state for BBD simulation
    std::vector<std::vector<float>> bbBuffers;  // Per-channel BBD processing buffers
    std::vector<int> bbWriteIndices;            // Per-channel BBD write indices
    std::vector<double> clockPhases;            // Per-channel clock phases
    
    // Random number generation for noise simulation
    juce::Random randomEngine;
    
    // Filter coefficients for bandwidth simulation
    std::vector<std::vector<float>> filterStates;  // Per-channel filter state
    float filterCoeff{0.0f};                       // Low-pass filter coefficient
    
    // Constants
    static constexpr double MIN_CLOCK_RATE = 100.0;      // Minimum clock rate
    static constexpr double MAX_CLOCK_RATE = 50000.0;    // Maximum clock rate
    static constexpr double DEFAULT_CLOCK_RATE = 2000.0; // Default clock rate
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDelayLine)
};

} // namespace audio_plugin
