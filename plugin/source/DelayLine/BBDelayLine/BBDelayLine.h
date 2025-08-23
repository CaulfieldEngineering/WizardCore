#pragma once

#include "../DelayLine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

namespace audio_plugin
{

/**
 * Bucket‑Brigade Delay (BBD) emulation with fixed stage count and variable clock.
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
 */
class BBDelayLine final : public DelayLine
{
public:
    BBDelayLine();
    ~BBDelayLine() override = default;

    //=== DelayLine interface =====================================================
    void prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels) override;
    void setDelayTime(double delayTimeInSeconds) override;           // Smoothed
    void setDelayInSamples(double delayInSamples) override;          // Smoothed
    void setDelayTimeImmediate(double delayTimeInSeconds) override;  // Hard set
    void setSmoothingTime(double rampTimeInSeconds) override;
    double getDelayTime() const override;                // target (seconds)
    double getDelayInSamples() const override;           // target (samples)
    double getCurrentDelayInSamples() const override;    // instantaneous from smoothed clock
    void setInterpolationType(InterpolationType type) override; // ignored for BBD
    InterpolationType getInterpolationType() const override; // always returns None for BBD
    void processBlock(juce::AudioBuffer<float>& buffer) override;
    void processBlock(juce::AudioBuffer<float>& buffer, float wetMix) override;
    float processSample(int channel, float input) override;
    bool isPrepared() const override;
    double getMaxDelayTime() const override;
    int getMaxDelayInSamples() const override;
    double getSampleRate() const override;
    void clear() override;

    //=== BBD parameters ==========================================================
    
    /**
     * @brief Set the number of BBD stages (emulating different chip types)
     * @param stages Number of stages (128-4096, default 1024 for MN3007)
     */
    void setStageCount(int stages);
    
    /**
     * @brief Set the droop/leak coefficient (capacitor discharge simulation)
     * @param droop Droop factor (0.0-1.0, where 1.0 = no droop, 0.95 = typical)
     */
    void setDroopFactor(float droop);
    
    /**
     * @brief Enable/disable anti-aliasing filters
     * @param enabled Whether to use input/output filtering
     */
    void setFilteringEnabled(bool enabled);

    // Getters for BBD parameters
    int getStageCount() const { return numStages; }
    float getDroopFactor() const { return droopFactor; }
    bool isFilteringEnabled() const { return filteringEnabled; }

private:
    // BBD emulation constants
    static constexpr int DEFAULT_STAGES = 256;    // More practical for chorus-range delays
    static constexpr int MIN_STAGES = 128;
    static constexpr int MAX_STAGES = 4096;
    static constexpr float DEFAULT_DROOP = 0.9995f; // Gentle droop per stage
    static constexpr double DEFAULT_SMOOTHING_TIME = 0.02;  // 20ms
    static constexpr double MIN_DELAY_TIME = 0.001;  // 1ms minimum
    
    // Core BBD parameters
    int numStages = DEFAULT_STAGES;
    float droopFactor = DEFAULT_DROOP;
    bool filteringEnabled = true;
    
    // State tracking
    std::atomic<bool> prepared{false};
    double sampleRate = 44100.0;
    int numChannels = 0;
    double maxDelayTimeSeconds = 0.0;
    
    // Delay time control
    double targetDelayTimeSeconds = 0.03;  // 30ms default
    double currentClockFreq = 44100.0;     // Current BBD clock frequency
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> smoothedClockFreq;
    
    // BBD stage arrays (one per channel)
    std::vector<std::vector<float>> stageBuffers;  // [channel][stage]
    
    // Clock management
    std::vector<double> clockAccumulators;  // Per-channel clock phase
    
    // Anti-aliasing filters (simple one-pole LPF)
    struct SimpleFilter {
        float state = 0.0f;
        float coefficient = 0.7f;  // Cutoff related to clock frequency
        
        float process(float input) {
            state += coefficient * (input - state);
            return state;
        }
        
        void setCutoff(float cutoffRatio) {
            coefficient = std::clamp(cutoffRatio, 0.1f, 0.9f);
        }
        
        void reset() {
            state = 0.0f;
        }
    };
    
    std::vector<SimpleFilter> inputFilters;   // One per channel
    std::vector<SimpleFilter> outputFilters;  // One per channel
    
    // Helper methods
    void updateClockFrequency();
    void updateFilterCutoffs();
    float processStages(int channel, float input);
    void resetStages();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDelayLine)
};

} // namespace audio_plugin
