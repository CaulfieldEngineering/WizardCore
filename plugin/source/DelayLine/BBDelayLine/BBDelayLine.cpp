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
    
    // Prepare internal core delay to maintain stable timing
    coreDelay = std::make_unique<DigitalDelayLine>();
    coreDelay->prepare(sampleRate, maxDelayTimeInSeconds, numChannels);
    coreDelay->setInterpolationType(DelayLine::InterpolationType::Linear);
    
    // Set initial delay time and update clock frequency
    updateClockFrequency();
    updateFilterCutoffs();
    
    prepared.store(true);
    
    // Now that we're prepared, apply any stored delay time
    if (targetDelayTimeSeconds > 0.0)
    {
        updateClockFrequency();
        coreDelay->setDelayTime(targetDelayTimeSeconds);
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
        if (coreDelay) coreDelay->setDelayTime(targetDelayTimeSeconds);
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
    if (coreDelay) coreDelay->setDelayTimeImmediate(targetDelayTimeSeconds);
}

void BBDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    smoothedClockFreq.reset(sampleRate, rampTimeInSeconds);
    if (coreDelay) coreDelay->setSmoothingTime(rampTimeInSeconds);
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
    if (coreDelay && coreDelay->isPrepared())
        return coreDelay->getCurrentDelayInSamples();
    
    // Fallback: Calculate current delay based on smoothed clock frequency
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
    float filteredInput = filteringEnabled ? inputFilters[channel].processSingleSampleRaw(input) : input;
    
    // Use stable timing core delay to compute delayed value at current delay setting
    float delayedCore = coreDelay ? coreDelay->processSample(channel, filteredInput) : filteredInput;
    
    // Impose BBD transfer droop by running through lightweight stage model without clock shifts
    // Approximate cumulative droop over N stages by N small one-tap leaks on each sample
    // Use a compact approximation: y = delayedCore * pow(droopFactor, numStages * 0.25f)
    // (0.25 reduces over-attenuation compared to full-stage cascade)
    const float droopGain = std::pow(droopFactor, static_cast<float>(numStages) * 0.25f);
    float output = delayedCore * droopGain;
    
    // Apply output filtering if enabled
    float filteredOutput = filteringEnabled ? outputFilters[channel].processSingleSampleRaw(output) : output;
    
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
    
    if (coreDelay) coreDelay->clear();
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
        
    // Set LPF cutoff based on the effective BBD bandwidth (~ f_clk / 2), but
    // relax slightly to preserve articulation; add a lower bound to avoid over-muffling.
    const double currentClock = smoothedClockFreq.getCurrentValue();
    const double targetCutoffHz = std::clamp(currentClock * 0.35, 3500.0, 11000.0);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto lpf = juce::IIRCoefficients::makeLowPass(sampleRate, targetCutoffHz, 0.707f);
        inputFilters[ch].setCoefficients(lpf);
        outputFilters[ch].setCoefficients(lpf);
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
