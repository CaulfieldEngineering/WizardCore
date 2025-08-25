#include "BBDelayLine.h"
#include "../DigitalDelayLine/DigitalDelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin
{

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

BBDelayLine::BBDelayLine()
{
    // Initialize smoothed clock frequency with default values
    smoothedClockFreq.reset(44100.0, DEFAULT_SMOOTHING_TIME);
    smoothedClockFreq.setCurrentAndTargetValue(44100.0);
}

// ============================================================================
// PREPARATION & SETUP
// ============================================================================

void BBDelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds, int newNumChannels)
{
    jassert(newSampleRate > 0);
    jassert(maxDelayTimeInSeconds > 0);
    jassert(newNumChannels > 0);
    
    // Store system parameters
    sampleRateHz.store(newSampleRate);
    numChannels.store(newNumChannels);
    maxDelayTimeSeconds.store(maxDelayTimeInSeconds);
    
    // Initialize smoothed clock frequency with new sample rate
    smoothedClockFreq.reset(newSampleRate, config.smoothingTimeInSeconds.load());
    
    // Allocate stage buffers for each channel
    stageBuffers.resize(newNumChannels);
    clockAccumulators.resize(newNumChannels);
    inputFilters.resize(newNumChannels);
    outputFilters.resize(newNumChannels);
    
    for (int ch = 0; ch < newNumChannels; ++ch)
    {
        stageBuffers[ch].resize(config.stageCount.load());
        std::fill(stageBuffers[ch].begin(), stageBuffers[ch].end(), 0.0f);
        clockAccumulators[ch] = 0.0;
        inputFilters[ch].reset();
        outputFilters[ch].reset();
    }
    
    // Prepare internal core delay to maintain stable timing
    coreDelay = std::make_unique<DigitalDelayLine>();
    coreDelay->prepare(newSampleRate, maxDelayTimeInSeconds, newNumChannels);
    coreDelay->setInterpolationType(DelayLine::InterpolationType::Linear);
    
    // Set initial delay time and update clock frequency
    updateClockFrequency();
    updateFilterCutoffs();
    
    prepared.store(true);
    
    // Now that we're prepared, apply any stored delay time
    if (config.delayTimeInSeconds.load() > 0.0)
    {
        updateClockFrequency();
        coreDelay->setDelayTimeInSeconds(config.delayTimeInSeconds.load(), true);
    }
}

// ============================================================================
// DELAYLINE INTERFACE IMPLEMENTATION
// ============================================================================

void BBDelayLine::setDelayTimeInSeconds(double delayTimeInSeconds, bool withSmoothing)
{
    // Store the target delay time even if not prepared yet
    // It will be applied when prepare() is called
    const double clampedDelay = std::clamp(delayTimeInSeconds, MIN_DELAY_TIME, maxDelayTimeSeconds.load());
    config.delayTimeInSeconds.store(clampedDelay);
    
    // Only update clock frequency if prepared
    if (isPrepared())
    {
        updateClockFrequency();
        if (coreDelay) 
        {
            coreDelay->setDelayTimeInSeconds(clampedDelay, withSmoothing);
        }
    }
}

void BBDelayLine::setDelayInSamples(double delayInSamples, bool withSmoothing)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    const double delayTimeInSeconds = delayInSamples / sampleRateHz.load();
    setDelayTimeInSeconds(delayTimeInSeconds, withSmoothing);
}

void BBDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    config.smoothingTimeInSeconds.store(rampTimeInSeconds);
    smoothedClockFreq.reset(sampleRateHz.load(), rampTimeInSeconds);
    
    if (coreDelay) 
    {
        coreDelay->setSmoothingTime(rampTimeInSeconds);
    }
}

double BBDelayLine::getDelayTimeInSeconds() const
{
    return config.delayTimeInSeconds.load();
}

double BBDelayLine::getDelayInSamples() const
{
    return config.delayTimeInSeconds.load() * sampleRateHz.load();
}

double BBDelayLine::getCurrentDelayInSamples() const
{
    if (coreDelay && coreDelay->isPrepared())
    {
        return coreDelay->getCurrentDelayInSamples();
    }
    
    // Fallback: Calculate current delay based on smoothed clock frequency
    if (smoothedClockFreq.getCurrentValue() <= 0.0)
    {
        return 0.0;
    }
        
    const double currentDelayTime = static_cast<double>(config.stageCount.load()) / smoothedClockFreq.getCurrentValue();
    return currentDelayTime * sampleRateHz.load();
}

void BBDelayLine::setInterpolationType(InterpolationType type)
{
    // BBD doesn't use interpolation - it's inherently discrete
    juce::ignoreUnused(type);
}

DelayLine::InterpolationType BBDelayLine::getInterpolationType() const
{
    return InterpolationType::None;  // BBD is always discrete
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

float BBDelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels.load());
    
    // Early return if disabled
    if (!config.enabled.load())
    {
        return input;
    }
    
    // Apply input filtering if enabled
    float filteredInput = config.filteringEnabled.load() ? 
        inputFilters[channel].processSingleSampleRaw(input) : input;
    
    // Use stable timing core delay to compute delayed value at current delay setting
    float delayedCore = coreDelay ? coreDelay->processSample(channel, filteredInput) : filteredInput;
    
    // Impose BBD transfer droop by running through lightweight stage model without clock shifts
    // Approximate cumulative droop over N stages by N small one-tap leaks on each sample
    // Use a compact approximation: y = delayedCore * pow(droopFactor, numStages * 0.25f)
    // (0.25 reduces over-attenuation compared to full-stage cascade)
    const float droopGain = std::pow(config.droopFactor.load(), 
        static_cast<float>(config.stageCount.load()) * 0.25f);
    float output = delayedCore * droopGain;
    
    // Apply output filtering if enabled
    float filteredOutput = config.filteringEnabled.load() ? 
        outputFilters[channel].processSingleSampleRaw(output) : output;
    
    return filteredOutput;
}

void BBDelayLine::processBlock(juce::AudioBuffer<float>& buffer)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels.load());
    
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = processSample(ch, channelData[i]);
        }
    }
}

void BBDelayLine::processBlock(juce::AudioBuffer<float>& buffer, float wetMix)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels.load());
    const float dryMix = 1.0f - wetMix;
    
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            const float input = channelData[i];
            const float delayed = processSample(ch, input);
            
            // Mix dry and wet signals
            channelData[i] = (input * dryMix) + (delayed * wetMix);
        }
    }
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void BBDelayLine::clear()
{
    for (auto& channelStages : stageBuffers)
    {
        std::fill(channelStages.begin(), channelStages.end(), 0.0f);
    }
    
    std::fill(clockAccumulators.begin(), clockAccumulators.end(), 0.0);
    
    for (auto& filter : inputFilters)
    {
        filter.reset();
    }
    
    for (auto& filter : outputFilters)
    {
        filter.reset();
    }
    
    if (coreDelay) 
    {
        coreDelay->clear();
    }
}

bool BBDelayLine::isPrepared() const
{
    return prepared.load();
}

double BBDelayLine::getMaxDelayTime() const
{
    return maxDelayTimeSeconds.load();
}

int BBDelayLine::getMaxDelayInSamples() const
{
    return static_cast<int>(maxDelayTimeSeconds.load() * sampleRateHz.load());
}

double BBDelayLine::getSampleRate() const
{
    return sampleRateHz.load();
}

// ============================================================================
// INDIVIDUAL PARAMETER SETTERS
// ============================================================================

void BBDelayLine::setStageCount(int stages)
{
    const int newStages = std::clamp(stages, MIN_STAGES, MAX_STAGES);
    
    if (newStages != config.stageCount.load())
    {
        config.stageCount.store(newStages);
        
        // Reallocate stage buffers if already prepared
        if (isPrepared())
        {
            for (int ch = 0; ch < numChannels.load(); ++ch)
            {
                stageBuffers[ch].resize(newStages);
                std::fill(stageBuffers[ch].begin(), stageBuffers[ch].end(), 0.0f);
            }
            
            updateClockFrequency();
        }
    }
}

void BBDelayLine::setDroopFactor(float droop)
{
    const float clampedDroop = std::clamp(droop, 0.0f, 1.0f);
    config.droopFactor.store(clampedDroop);
}

void BBDelayLine::setFilteringEnabled(bool enabled)
{
    config.filteringEnabled.store(enabled);
    
    if (isPrepared())
    {
        updateFilterCutoffs();
    }
}

void BBDelayLine::setEnabled(bool enabled)
{
    config.enabled.store(enabled);
}

// ============================================================================
// BATCH PARAMETER UPDATES
// ============================================================================

void BBDelayLine::updateParameters(int stages, float droop, bool filtering, bool enabled)
{
    setStageCount(stages);
    setDroopFactor(droop);
    setFilteringEnabled(filtering);
    setEnabled(enabled);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void BBDelayLine::updateClockFrequency()
{
    // Calculate required clock frequency to achieve target delay time
    // Clock frequency = numStages / targetDelayTime
    const double targetDelay = config.delayTimeInSeconds.load();
    
    if (targetDelay > 0.0)
    {
        const double newClockFreq = static_cast<double>(config.stageCount.load()) / targetDelay;
        
        // Clamp to reasonable range (avoid aliasing and ensure stability)
        const double maxClockFreq = sampleRateHz.load() * 0.4;  // Nyquist-safe
        const double minClockFreq = static_cast<double>(config.stageCount.load()) / maxDelayTimeSeconds.load();
        
        const double clampedClockFreq = std::clamp(newClockFreq, minClockFreq, maxClockFreq);
        currentClockFreq.store(clampedClockFreq);
        
        // Set the smoothed target
        smoothedClockFreq.setTargetValue(clampedClockFreq);
    }
}

void BBDelayLine::updateFilterCutoffs()
{
    if (!config.filteringEnabled.load())
    {
        return;
    }
        
    // Set LPF cutoff based on the effective BBD bandwidth (~ f_clk / 2), but
    // relax slightly to preserve articulation; add a lower bound to avoid over-muffling.
    const double currentClock = smoothedClockFreq.getCurrentValue();
    const double targetCutoffHz = std::clamp(currentClock * 0.35, 3500.0, 11000.0);
    
    for (int ch = 0; ch < numChannels.load(); ++ch)
    {
        auto lpf = juce::IIRCoefficients::makeLowPass(sampleRateHz.load(), targetCutoffHz, 0.707f);
        inputFilters[ch].setCoefficients(lpf);
        outputFilters[ch].setCoefficients(lpf);
    }
}

float BBDelayLine::processStages(int channel, float input)
{
    auto& stages = stageBuffers[channel];
    auto& clockAccum = clockAccumulators[channel];
    
    // Get current smoothed clock frequency
    const double clockFreq = smoothedClockFreq.getNextValue();
    
    // Calculate clock increment per sample
    const double clockIncrement = clockFreq / sampleRateHz.load();
    
    // Advance clock accumulator
    clockAccum += clockIncrement;
    
    // Check if we need to shift stages (clock tick)
    while (clockAccum >= 1.0)
    {
        clockAccum -= 1.0;
        
        // Shift all stages (from last to first)
        for (int stage = config.stageCount.load() - 1; stage > 0; --stage)
        {
            // Apply droop/leak during transfer
            stages[stage] = stages[stage - 1] * config.droopFactor.load();
        }
        
        // Input goes to first stage
        stages[0] = input;
    }
    
    // Output comes from the last stage
    return stages[config.stageCount.load() - 1];
}

void BBDelayLine::resetStages()
{
    for (auto& channelStages : stageBuffers)
    {
        std::fill(channelStages.begin(), channelStages.end(), 0.0f);
    }
}

} // namespace audio_plugin
