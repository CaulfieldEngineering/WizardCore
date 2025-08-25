#pragma once

#include "../DelayLine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <memory>
#include <atomic>

// Forward declaration
namespace audio_plugin { class DigitalDelayLine; }

namespace audio_plugin
{

/**
 * @brief Bucket‑Brigade Delay (BBD) emulation with fixed stage count and variable clock
 *
 * Key behaviors modeled:
 *  - Fixed number of stages (e.g., MN3007 ≈ 1024) shifted by a variable clock
 *  - Two‑phase (φ1/φ2) clock option: effective sampling f_eff = f_clk / 2
 *  - Per‑transfer droop/leak (capacitor discharge) parameterized by a time constant τ
 *  - Essential pre/post low‑pass filters tied to f_eff (anti‑alias & reconstruction)
 *  - Shared clock for all channels, advanced once per frame (no per‑channel drift)
 *  - Multi‑tick handling when f_clk / fs_host > 1 (perform multiple shifts per host sample)
 *
 * This class derives from DelayLine to be drop‑in compatible with your existing factory.
 * 
 * Thread Safety: Thread-safe for parallel channel processing
 * Memory: Allocates stage buffers and filters per channel
 */
class BBDelayLine final : public DelayLine
{
public:
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
        // Core BBD sonic parameters
        std::atomic<int> stageCount{256};                    ///< Number of BBD stages (128-4096)
        std::atomic<float> droopFactor{0.9995f};             ///< Droop/leak factor per stage [0.0, 1.0]
        std::atomic<bool> filteringEnabled{true};            ///< Anti-aliasing filter state
        std::atomic<double> delayTimeInSeconds{0.03};        ///< Target delay time in seconds
        std::atomic<double> smoothingTimeInSeconds{0.02};    ///< Smoothing ramp time in seconds
        
        // Musical/sync parameters
        std::atomic<bool> enabled{true};                     ///< Enabled state
    };

    // ============================================================================
    // CONSTRUCTOR & DESTRUCTOR
    // ============================================================================
    
    /**
     * @brief Default constructor
     */
    BBDelayLine();
    
    /**
     * @brief Destructor
     */
    ~BBDelayLine() override = default;

    // ============================================================================
    // PUBLIC INTERFACE
    // ============================================================================
    
    // Access to sonic configuration for debugging/monitoring (READ-ONLY)
    const Config& getConfig() const { return config; }
    
    // ============================================================================
    // DELAYLINE INTERFACE IMPLEMENTATION
    // ============================================================================
    
    /**
     * @brief Prepare the delay line for processing
     * @param sampleRate The sample rate in Hz
     * @param maxDelayTimeInSeconds Maximum delay time in seconds
     * @param numChannels Number of audio channels to process
     */
    void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels) override;
    
    /**
     * @brief Set the delay time for all channels
     * @param delayTimeInSeconds Delay time in seconds (will be clamped to max)
     */
    void setDelayTimeInSeconds(double delayTimeInSeconds, bool withSmoothing = true) override;
    
    /**
     * @brief Set the delay time with sample-accurate precision
     * @param delayInSamples Delay in samples (can be fractional)
     */
    void setDelayInSamples(double delayInSamples, bool withSmoothing = true) override;
    
    /**
     * @brief Set the smoothing ramp time for delay changes
     * @param rampTimeInSeconds Time in seconds for delay changes to ramp
     */
    void setSmoothingTime(double rampTimeInSeconds) override;
    
    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    double getDelayTime() const override;
    
    /**
     * @brief Get the current delay time in samples
     * @return Current delay time in samples (can be fractional)
     */
    double getDelayInSamples() const override;
    
    /**
     * @brief Get the current actual delay in samples (may differ from set value during smoothing)
     * @return Current actual delay in samples
     */
    double getCurrentDelayInSamples() const override;
    
    /**
     * @brief Set the interpolation type for fractional delay support
     * @param type The interpolation type to use (ignored for BBD)
     */
    void setInterpolationType(InterpolationType type) override;
    
    /**
     * @brief Get the current interpolation type
     * @return Always returns InterpolationType::None for BBD
     */
    InterpolationType getInterpolationType() const override;
    
    /**
     * @brief Process a single sample through the delay line
     * @param channel The channel to process (0-based)
     * @param input The input sample
     * @return The delayed output sample
     */
    float processSample(int channel, float input) override;
    
    /**
     * @brief Process an entire audio block
     * @param buffer The audio buffer to process
     */
    void processBlock(juce::AudioBuffer<float>& buffer) override;
    
    /**
     * @brief Process an entire audio block with wet/dry mixing
     * @param buffer The audio buffer to process
     * @param wetMix Wet signal mix amount [0.0, 1.0]
     */
    void processBlock(juce::AudioBuffer<float>& buffer, float wetMix);
    
    /**
     * @brief Clear all delay buffers and reset state
     */
    void clear() override;
    
    /**
     * @brief Check if the delay line is ready for processing
     * @return true if prepare() has been called, false otherwise
     */
    bool isPrepared() const override;
    
    /**
     * @brief Get the maximum delay time in seconds
     * @return Maximum delay time in seconds
     */
    double getMaxDelayTime() const override;
    
    /**
     * @brief Get the maximum delay time in samples
     * @return Maximum delay time in samples
     */
    int getMaxDelayInSamples() const override;
    
    /**
     * @brief Get the current sample rate
     * @return Current sample rate in Hz
     */
    double getSampleRate() const override;

    // ============================================================================
    // INDIVIDUAL PARAMETER SETTERS
    // ============================================================================
    
    /**
     * @brief Set the number of BBD stages (emulating different chip types)
     * @param stages Number of stages (128-4096, default 256 for practical use)
     */
    void setStageCount(int stages);
    
    /**
     * @brief Set the droop/leak coefficient (capacitor discharge simulation)
     * @param droop Droop factor [0.0, 1.0] where 1.0 = no droop, 0.95 = typical
     */
    void setDroopFactor(float droop);
    
    /**
     * @brief Enable/disable anti-aliasing filters
     * @param enabled Whether to use input/output filtering
     */
    void setFilteringEnabled(bool enabled);
    
    /**
     * @brief Set the enabled state
     * @param enabled Whether the delay line is active
     */
    void setEnabled(bool enabled);

    // ============================================================================
    // INDIVIDUAL PARAMETER GETTERS
    // ============================================================================
    
    /**
     * @brief Get the current stage count
     * @return Number of BBD stages
     */
    int getStageCount() const { return config.stageCount.load(); }
    
    /**
     * @brief Get the current droop factor
     * @return Droop factor value
     */
    float getDroopFactor() const { return config.droopFactor.load(); }
    
    /**
     * @brief Check if filtering is enabled
     * @return true if anti-aliasing filters are active
     */
    bool isFilteringEnabled() const { return config.filteringEnabled.load(); }
    
    /**
     * @brief Check if the delay line is enabled
     * @return true if the delay line is active
     */
    bool isEnabled() const { return config.enabled.load(); }
    
    /**
     * @brief Get the current delay time in seconds
     * @return Current delay time in seconds
     */
    double getDelayTimeInSeconds() const { return config.delayTimeInSeconds.load(); }
    
    /**
     * @brief Get the current smoothing time in seconds
     * @return Current smoothing ramp time in seconds
     */
    double getSmoothingTimeInSeconds() const { return config.smoothingTimeInSeconds.load(); }

    // ============================================================================
    // BATCH PARAMETER UPDATES
    // ============================================================================
    
    /**
     * @brief Update all BBD-specific parameters at once
     * @param stages Number of BBD stages
     * @param droop Droop factor [0.0, 1.0]
     * @param filtering Whether to enable anti-aliasing filters
     * @param enabled Whether the delay line is active
     */
    void updateParameters(int stages, float droop, bool filtering, bool enabled);

private:
    // ============================================================================
    // PRIVATE MEMBER VARIABLES
    // ============================================================================
    
    // Sonic parameters (what makes this module sound/behave differently)
    Config config;                                    // PRIVATE - internal storage
    
    // System/technical parameters (not sonic)
    std::atomic<bool> prepared{false};                // PRIVATE - implementation state
    std::atomic<double> sampleRateHz{44100.0};        // PRIVATE - system value
    std::atomic<int> numChannels{0};                  // PRIVATE - system value
    std::atomic<double> maxDelayTimeSeconds{0.0};     // PRIVATE - system value
    
    // BBD emulation constants
    static constexpr int DEFAULT_STAGES = 256;        // More practical for chorus-range delays
    static constexpr int MIN_STAGES = 128;
    static constexpr int MAX_STAGES = 4096;
    static constexpr float DEFAULT_DROOP = 0.9995f;   // Gentle droop per stage
    static constexpr double DEFAULT_SMOOTHING_TIME = 0.02;  // 20ms
    static constexpr double MIN_DELAY_TIME = 0.001;   // 1ms minimum
    
    // Delay time control
    std::atomic<double> currentClockFreq{44100.0};    // PRIVATE - current BBD clock frequency
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoothedClockFreq;
    
    // BBD stage arrays (one per channel)
    std::vector<std::vector<float>> stageBuffers;     // PRIVATE - [channel][stage]
    
    // Clock management
    std::vector<double> clockAccumulators;            // PRIVATE - per-channel clock phase
    
    // Anti-aliasing filters (2nd-order low-pass per channel)
    std::vector<juce::IIRFilter> inputFilters;        // PRIVATE - one per channel
    std::vector<juce::IIRFilter> outputFilters;       // PRIVATE - one per channel
    
    // Core interpolating delay engine to avoid pitch drift while retaining BBD coloration
    std::unique_ptr<DigitalDelayLine> coreDelay;     // PRIVATE - stable timing core

    // ============================================================================
    // PRIVATE HELPER METHODS
    // ============================================================================
    
    /**
     * @brief Update the BBD clock frequency based on current delay time
     */
    void updateClockFrequency();
    
    /**
     * @brief Update filter cutoff frequencies based on current clock
     */
    void updateFilterCutoffs();
    
    /**
     * @brief Process audio through BBD stages (legacy method, not currently used)
     * @param channel Channel index
     * @param input Input sample
     * @return Processed output sample
     */
    float processStages(int channel, float input);
    
    /**
     * @brief Reset all BBD stage buffers to zero
     */
    void resetStages();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDelayLine)
};

} // namespace audio_plugin
