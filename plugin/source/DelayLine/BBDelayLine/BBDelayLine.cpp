#include "BBDelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

BBDelayLine::BBDelayLine()
{
    // Initialize smoothed clock frequency
    smoothedClockFreq.reset(44100.0, DEFAULT_SMOOTHING_TIME);
    smoothedClockFreq.setCurrentAndTargetValue(44100.0);
}

void BBDelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds, int newNumChannels)
{
    jassert(newSampleRate > 0);
    jassert(maxDelayTimeInSeconds > 0);
    jassert(newNumChannels > 0);
    
    sampleRate = newSampleRate;
    numChannels = newNumChannels;
    maxDelayTimeSeconds = maxDelayTimeInSeconds;
    
    // Initialize smoothed clock frequency with new sample rate
    smoothedClockFreq.reset(sampleRate, DEFAULT_SMOOTHING_TIME);
    
    // Allocate stage buffers for each channel
    stageBuffers.resize(numChannels);
    clockAccumulators.resize(numChannels);
    inputFilters.resize(numChannels);
    outputFilters.resize(numChannels);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        stageBuffers[ch].resize(numStages);
        std::fill(stageBuffers[ch].begin(), stageBuffers[ch].end(), 0.0f);
        clockAccumulators[ch] = 0.0;
        inputFilters[ch].reset();
        outputFilters[ch].reset();
    }
    
    // Set initial delay time and update clock frequency
    updateClockFrequency();
    updateFilterCutoffs();
    
    prepared.store(true);
    
    // Now that we're prepared, apply any stored delay time
    if (targetDelayTimeSeconds > 0.0)
    {
        updateClockFrequency();
    }
}

void BBDelayLine::setDelayTime(double delayTimeInSeconds)
{
    // Store the target delay time even if not prepared yet
    // It will be applied when prepare() is called
    targetDelayTimeSeconds = std::clamp(delayTimeInSeconds, MIN_DELAY_TIME, maxDelayTimeSeconds);
    
    // Only update clock frequency if prepared
    if (isPrepared())
    {
        updateClockFrequency();
    }
}

void BBDelayLine::setDelayInSamples(double delayInSamples)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    double delayTimeInSeconds = delayInSamples / sampleRate;
    setDelayTime(delayTimeInSeconds);
}

void BBDelayLine::setDelayTimeImmediate(double delayTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    targetDelayTimeSeconds = std::clamp(delayTimeInSeconds, MIN_DELAY_TIME, maxDelayTimeSeconds);
    updateClockFrequency();
    
    // Set clock frequency immediately without smoothing
    smoothedClockFreq.setCurrentAndTargetValue(currentClockFreq);
    updateFilterCutoffs();
}

void BBDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    smoothedClockFreq.reset(sampleRate, rampTimeInSeconds);
}

double BBDelayLine::getDelayTime() const
{
    return targetDelayTimeSeconds;
}

double BBDelayLine::getDelayInSamples() const
{
    return targetDelayTimeSeconds * sampleRate;
}

double BBDelayLine::getCurrentDelayInSamples() const
{
    // Calculate current delay based on smoothed clock frequency
    if (smoothedClockFreq.getCurrentValue() <= 0.0)
        return 0.0;
        
    double currentDelayTime = static_cast<double>(numStages) / smoothedClockFreq.getCurrentValue();
    return currentDelayTime * sampleRate;
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

float BBDelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels);
    
    // Apply input filtering if enabled
    float filteredInput = filteringEnabled ? inputFilters[channel].process(input) : input;
    
    // Process through BBD stages
    float output = processStages(channel, filteredInput);
    
    // Apply output filtering if enabled
    float filteredOutput = filteringEnabled ? outputFilters[channel].process(output) : output;
    
    return filteredOutput;
}

void BBDelayLine::processBlock(juce::AudioBuffer<float>& buffer)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels);
    
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
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels);
    float dryMix = 1.0f - wetMix;
    
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float input = channelData[i];
            float delayed = processSample(ch, input);
            
            // Mix dry and wet signals
            channelData[i] = (input * dryMix) + (delayed * wetMix);
        }
    }
}

void BBDelayLine::clear()
{
    for (auto& channelStages : stageBuffers)
    {
        std::fill(channelStages.begin(), channelStages.end(), 0.0f);
    }
    
    std::fill(clockAccumulators.begin(), clockAccumulators.end(), 0.0);
    
    for (auto& filter : inputFilters)
        filter.reset();
    for (auto& filter : outputFilters)
        filter.reset();
}

bool BBDelayLine::isPrepared() const
{
    return prepared.load();
}

double BBDelayLine::getMaxDelayTime() const
{
    return maxDelayTimeSeconds;
}

int BBDelayLine::getMaxDelayInSamples() const
{
    return static_cast<int>(maxDelayTimeSeconds * sampleRate);
}

double BBDelayLine::getSampleRate() const
{
    return sampleRate;
}

// BBD-specific parameter setters
void BBDelayLine::setStageCount(int stages)
{
    int newStages = std::clamp(stages, MIN_STAGES, MAX_STAGES);
    
    if (newStages != numStages)
    {
        numStages = newStages;
        
        // Reallocate stage buffers if already prepared
        if (isPrepared())
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                stageBuffers[ch].resize(numStages);
                std::fill(stageBuffers[ch].begin(), stageBuffers[ch].end(), 0.0f);
            }
            
            updateClockFrequency();
        }
    }
}

void BBDelayLine::setDroopFactor(float droop)
{
    droopFactor = std::clamp(droop, 0.0f, 1.0f);
}

void BBDelayLine::setFilteringEnabled(bool enabled)
{
    filteringEnabled = enabled;
    
    if (isPrepared())
    {
        updateFilterCutoffs();
    }
}

// Private helper methods
void BBDelayLine::updateClockFrequency()
{
    // Calculate required clock frequency to achieve target delay time
    // Clock frequency = numStages / targetDelayTime
    if (targetDelayTimeSeconds > 0.0)
    {
        currentClockFreq = static_cast<double>(numStages) / targetDelayTimeSeconds;
        
        // Clamp to reasonable range (avoid aliasing and ensure stability)
        double maxClockFreq = sampleRate * 0.4;  // Nyquist-safe
        double minClockFreq = static_cast<double>(numStages) / maxDelayTimeSeconds;
        
        currentClockFreq = std::clamp(currentClockFreq, minClockFreq, maxClockFreq);
        
        // Set the smoothed target
        smoothedClockFreq.setTargetValue(currentClockFreq);
    }
}

void BBDelayLine::updateFilterCutoffs()
{
    if (!filteringEnabled)
        return;
        
    // Set filter cutoff relative to current clock frequency
    // Use a conservative cutoff to prevent aliasing
    double currentClock = smoothedClockFreq.getCurrentValue();
    double cutoffFreq = currentClock * 0.3;  // 30% of clock frequency
    double cutoffRatio = std::clamp(cutoffFreq / sampleRate, 0.1, 0.9);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        inputFilters[ch].setCutoff(static_cast<float>(cutoffRatio));
        outputFilters[ch].setCutoff(static_cast<float>(cutoffRatio));
    }
}

float BBDelayLine::processStages(int channel, float input)
{
    auto& stages = stageBuffers[channel];
    auto& clockAccum = clockAccumulators[channel];
    
    // Get current smoothed clock frequency
    double clockFreq = smoothedClockFreq.getNextValue();
    
    // Calculate clock increment per sample
    double clockIncrement = clockFreq / sampleRate;
    
    // Advance clock accumulator
    clockAccum += clockIncrement;
    
    // Check if we need to shift stages (clock tick)
    while (clockAccum >= 1.0)
    {
        clockAccum -= 1.0;
        
        // Shift all stages (from last to first)
        for (int stage = numStages - 1; stage > 0; --stage)
        {
            // Apply droop/leak during transfer
            stages[stage] = stages[stage - 1] * droopFactor;
        }
        
        // Input goes to first stage
        stages[0] = input;
    }
    
    // Output comes from the last stage
    return stages[numStages - 1];
}

void BBDelayLine::resetStages()
{
    for (auto& channelStages : stageBuffers)
    {
        std::fill(channelStages.begin(), channelStages.end(), 0.0f);
    }
}

} // namespace audio_plugin
