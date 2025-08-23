#include "BBDelayLine.h"
#include <algorithm>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>

namespace audio_plugin {

BBDelayLine::BBDelayLine()
    : randomEngine()
{
}

void BBDelayLine::prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels)
{
    // Store base parameters
    this->sampleRate = sampleRate;
    this->maxDelayTimeInSeconds = maxDelayTimeInSeconds;
    this->maxDelayInSamples = static_cast<int>(sampleRate * maxDelayTimeInSeconds);
    this->numChannels = numChannels;
    
    // Initialize delay buffers
    delayBuffers.resize(numChannels);
    writeIndices.resize(numChannels);
    readPositions.resize(numChannels);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        delayBuffers[ch].resize(maxDelayInSamples + 2, 0.0f);
        writeIndices[ch] = 0;
        readPositions[ch] = 0.0;
    }
    
    // Initialize BBD-specific buffers and state
    bbBuffers.resize(numChannels);
    bbWriteIndices.resize(numChannels);
    clockPhases.resize(numChannels);
    filterStates.resize(numChannels);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        // BBD buffers are smaller than main delay buffers (simulating lower internal sample rate)
        int bbBufferSize = juce::roundToInt(clockRate / sampleRate * maxDelayInSamples) + 2;
        bbBuffers[ch].resize(bbBufferSize);
        std::fill(bbBuffers[ch].begin(), bbBuffers[ch].end(), 0.0f);
        
        bbWriteIndices[ch] = 0;
        clockPhases[ch] = 0.0;
        filterStates[ch].resize(2, 0.0f); // 2-pole filter state
    }
    
    // Initialize smoothed delay
    smoothedDelay.reset(sampleRate, 0.01); // 10ms default smoothing
    
    // Calculate filter coefficient based on bandwidth reduction
    updateFilterCoefficient();
    
    // Mark as prepared
    prepared.store(true);
}

void BBDelayLine::setClockRate(double clockRateHz)
{
    clockRate = juce::jlimit(MIN_CLOCK_RATE, MAX_CLOCK_RATE, clockRateHz);
    
    // If already prepared, update BBD buffer sizes
    if (isPrepared())
    {
        int numChannels = static_cast<int>(bbBuffers.size());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            int bbBufferSize = juce::roundToInt(clockRate / getSampleRate() * getMaxDelayInSamples()) + 2;
            bbBuffers[ch].resize(bbBufferSize);
            std::fill(bbBuffers[ch].begin(), bbBuffers[ch].end(), 0.0f);
        }
    }
}

double BBDelayLine::getClockRate() const
{
    return clockRate;
}

void BBDelayLine::setNoiseAmount(double amount)
{
    noiseAmount = juce::jlimit(0.0, 1.0, amount);
}

double BBDelayLine::getNoiseAmount() const
{
    return noiseAmount;
}

void BBDelayLine::setBandwidthReduction(double reduction)
{
    bandwidthReduction = juce::jlimit(0.0, 1.0, reduction);
    updateFilterCoefficient();
}

double BBDelayLine::getBandwidthReduction() const
{
    return bandwidthReduction;
}

void BBDelayLine::setBBDCharacteristic(BBDCharacteristic newCharacteristic)
{
    characteristic = newCharacteristic;
    
    // Apply preset configurations
    switch (characteristic)
    {
        case BBDCharacteristic::Vintage:
            noiseAmount = 0.15;
            bandwidthReduction = 0.3;
            break;
        case BBDCharacteristic::Modern:
            noiseAmount = 0.05;
            bandwidthReduction = 0.15;
            break;
        case BBDCharacteristic::Dirty:
            noiseAmount = 0.4;
            bandwidthReduction = 0.6;
            break;
    }
    
    updateFilterCoefficient();
}

BBDelayLine::BBDCharacteristic BBDelayLine::getBBDCharacteristic() const
{
    return characteristic;
}

float BBDelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels);
    
    // First apply BBD processing to the input
    float bbProcessed = applyBBDProcessing(channel, input);
    
    // Write the BBD-processed input to the delay buffer
    auto& buffer = delayBuffers[channel];
    auto& writeIndex = writeIndices[channel];
    
    buffer[writeIndex] = bbProcessed;
    
    // Get the current delay value
    double currentDelay = smoothedDelay.getNextValue();
    
    // Calculate read position with wrap-around
    double readPos = static_cast<double>(writeIndex) - currentDelay;
    while (readPos < 0.0)
        readPos += buffer.size();
    
    int readIndex = static_cast<int>(std::round(readPos)) % static_cast<int>(buffer.size());
    
    // Read the delayed sample
    float output = buffer[readIndex];
    
    // Advance write index
    writeIndex = (writeIndex + 1) % static_cast<int>(buffer.size());
    
    return output;
}

void BBDelayLine::processBlock(juce::AudioBuffer<float>& buffer)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(bbBuffers.size()));
    
    // Update clock modulation for the entire block
    updateClockModulation();
    
    // Process each channel
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
    const int channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(bbBuffers.size()));
    float dryMix = 1.0f - wetMix;
    
    // Update clock modulation for the entire block
    updateClockModulation();
    
    // Process each channel
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
    // Clear delay buffers
    for (auto& buffer : delayBuffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    std::fill(writeIndices.begin(), writeIndices.end(), 0);
    std::fill(readPositions.begin(), readPositions.end(), 0.0);
    
    // Clear BBD-specific buffers
    for (auto& buffer : bbBuffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    std::fill(bbWriteIndices.begin(), bbWriteIndices.end(), 0);
    std::fill(clockPhases.begin(), clockPhases.end(), 0.0);
    
    for (auto& filterState : filterStates)
    {
        std::fill(filterState.begin(), filterState.end(), 0.0f);
    }
}

void BBDelayLine::setClockModulation(double modulationDepth, double modulationRate)
{
    this->modulationDepth = juce::jlimit(0.0, 1.0, modulationDepth);
    this->modulationRate = juce::jlimit(0.0, 20.0, modulationRate); // Max 20Hz modulation
}

void BBDelayLine::getClockModulation(double& depth, double& rate) const
{
    depth = modulationDepth;
    rate = modulationRate;
}

float BBDelayLine::applyBBDProcessing(int channel, float input)
{
    auto& bbBuffer = bbBuffers[channel];
    auto& bbWriteIndex = bbWriteIndices[channel];
    auto& clockPhase = clockPhases[channel];
    auto& filterState = filterStates[channel];
    
    // Calculate effective clock rate with modulation
    double effectiveClockRate = clockRate;
    if (modulationDepth > 0.0 && modulationRate > 0.0)
    {
        double modulation = std::sin(modulationPhase) * modulationDepth;
        effectiveClockRate *= (1.0 + modulation * 0.1); // 10% modulation range
    }
    
    // Advance clock phase
    clockPhase += effectiveClockRate / getSampleRate();
    
    // Only process when clock phase wraps (simulating BBD clock)
    if (clockPhase >= 1.0)
    {
        clockPhase -= 1.0;
        
        // Add clock noise/jitter
        if (noiseAmount > 0.0)
        {
            float noise = randomEngine.nextFloat() * 2.0f - 1.0f; // Range: -1.0 to 1.0
            noise *= noiseAmount * 0.1f;
            input += noise;
        }
        
        // Write to BBD buffer
        bbBuffer[bbWriteIndex] = input;
        bbWriteIndex = (bbWriteIndex + 1) % static_cast<int>(bbBuffer.size());
    }
    
    // Read from BBD buffer (with some clock jitter)
    int readIndex = bbWriteIndex;
    if (noiseAmount > 0.0)
    {
        float jitter = (randomEngine.nextFloat() * 2.0f - 1.0f) * noiseAmount * 0.05f;
        readIndex = (readIndex + static_cast<int>(jitter * bbBuffer.size())) % static_cast<int>(bbBuffer.size());
    }
    
    float output = bbBuffer[readIndex];
    
    // Apply bandwidth filtering
    output = applyBandwidthFilter(channel, output);
    
    return output;
}

void BBDelayLine::updateClockModulation()
{
    if (modulationRate > 0.0)
    {
        modulationPhase += juce::MathConstants<double>::twoPi * modulationRate / getSampleRate();
        if (modulationPhase >= juce::MathConstants<double>::twoPi)
            modulationPhase -= juce::MathConstants<double>::twoPi;
    }
}

float BBDelayLine::applyBandwidthFilter(int channel, float sample)
{
    if (bandwidthReduction <= 0.0)
        return sample;
    
    // Simple 2-pole low-pass filter to simulate BBD bandwidth limitations
    float output = sample * filterCoeff + 
                   filterStates[channel][0] * (1.0f - filterCoeff);
    
    filterStates[channel][0] = output;
    
    // Second pole for more aggressive filtering
    if (bandwidthReduction > 0.5)
    {
        output = output * filterCoeff + 
                 filterStates[channel][1] * (1.0f - filterCoeff);
        filterStates[channel][1] = output;
    }
    
    return output;
}

void BBDelayLine::updateFilterCoefficient()
{
    // Convert bandwidth reduction to filter coefficient
    // Higher reduction = lower cutoff frequency
    float cutoffFreq = 1.0f - static_cast<float>(bandwidthReduction);
    filterCoeff = juce::jmax(0.01f, cutoffFreq * 0.8f);
}

// Required DelayLine interface implementations
void BBDelayLine::setDelayTime(double delayTimeInSeconds)
{
    if (sampleRate <= 0)
        return;
    
    double delayInSamples = delayTimeInSeconds * sampleRate;
    setDelayInSamples(delayInSamples);
}

void BBDelayLine::setDelayInSamples(double delayInSamples)
{
    targetDelayInSamples = juce::jlimit(0.0, static_cast<double>(maxDelayInSamples), delayInSamples);
    smoothedDelay.setTargetValue(targetDelayInSamples);
}

void BBDelayLine::setDelayTimeImmediate(double delayTimeInSeconds)
{
    if (sampleRate <= 0)
        return;
    
    double delayInSamples = delayTimeInSeconds * sampleRate;
    targetDelayInSamples = juce::jlimit(0.0, static_cast<double>(maxDelayInSamples), delayInSamples);
    smoothedDelay.setCurrentAndTargetValue(targetDelayInSamples);
}

void BBDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (sampleRate > 0)
        smoothedDelay.reset(sampleRate, rampTimeInSeconds);
}

double BBDelayLine::getDelayTime() const
{
    if (sampleRate <= 0)
        return 0.0;
    return targetDelayInSamples / sampleRate;
}

double BBDelayLine::getDelayInSamples() const
{
    return targetDelayInSamples;
}

double BBDelayLine::getCurrentDelayInSamples() const
{
    return smoothedDelay.getCurrentValue();
}

void BBDelayLine::setInterpolationType(InterpolationType type)
{
    // BBD doesn't use interpolation - it's analog simulation
    (void)type; // Suppress unused parameter warning
}

DelayLine::InterpolationType BBDelayLine::getInterpolationType() const
{
    return InterpolationType::None; // BBD is always no interpolation
}

bool BBDelayLine::isPrepared() const
{
    return prepared.load();
}

double BBDelayLine::getMaxDelayTime() const
{
    return maxDelayTimeInSeconds;
}

int BBDelayLine::getMaxDelayInSamples() const
{
    return maxDelayInSamples;
}

double BBDelayLine::getSampleRate() const
{
    return sampleRate;
}

} // namespace audio_plugin
