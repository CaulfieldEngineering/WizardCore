#include "DelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

DelayLine::DelayLine()
{
}

DelayLine::~DelayLine() = default;

void DelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds, int newNumChannels)
{
    jassert(newSampleRate > 0);
    jassert(maxDelayTimeInSeconds > 0);
    jassert(newNumChannels > 0);
    
    sampleRate = newSampleRate;
    numChannels = newNumChannels;
    maxDelayInSamples = maxDelayTimeInSeconds * sampleRate;
    
    // Add extra samples for interpolation headroom
    bufferSize = static_cast<int>(std::ceil(maxDelayInSamples)) + 2;
    
    // Allocate per-channel buffers and indices
    buffers.resize(numChannels);
    writeIndices.resize(numChannels);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffers[ch].resize(bufferSize);
        std::fill(buffers[ch].begin(), buffers[ch].end(), 0.0f);
        writeIndices[ch] = 0;
    }
    
    // Initialize smoothing
    smoothedDelay.reset(sampleRate, DEFAULT_SMOOTHING_TIME);
    smoothedDelay.setCurrentAndTargetValue(0.0);
    targetDelayInSamples = 0.0;
    
    // Mark as prepared
    prepared.store(true);
}

void DelayLine::setDelayTime(double delayTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    double delayInSamples = delayTimeInSeconds * sampleRate;
    setDelayInSamples(delayInSamples);
}

void DelayLine::setDelayInSamples(double delayInSamples)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    // Clamp to valid range
    targetDelayInSamples = std::clamp(delayInSamples, MIN_DELAY_SAMPLES, maxDelayInSamples);
    smoothedDelay.setTargetValue(targetDelayInSamples);
}

void DelayLine::setDelayTimeImmediate(double delayTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    double delayInSamples = delayTimeInSeconds * sampleRate;
    targetDelayInSamples = std::clamp(delayInSamples, MIN_DELAY_SAMPLES, maxDelayInSamples);
    smoothedDelay.setCurrentAndTargetValue(targetDelayInSamples);
}

void DelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    smoothedDelay.reset(sampleRate, rampTimeInSeconds);
}

double DelayLine::getDelayTime() const
{
    if (sampleRate <= 0)
        return 0.0;
        
    return targetDelayInSamples / sampleRate;
}

double DelayLine::getDelayInSamples() const
{
    return targetDelayInSamples;
}

double DelayLine::getCurrentDelayInSamples() const
{
    return smoothedDelay.getCurrentValue();
}

void DelayLine::setInterpolationType(InterpolationType type)
{
    interpolationType = type;
}

DelayLine::InterpolationType DelayLine::getInterpolationType() const
{
    return interpolationType;
}

float DelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels);
    
    if (interpolationType == InterpolationType::Linear)
        return processSampleLinearInterp(channel, input);
    else
        return processSampleNoInterp(channel, input);
}

float DelayLine::processSampleLinearInterp(int channel, float input)
{
    auto& buffer = buffers[channel];
    auto& writeIndex = writeIndices[channel];
    
    // Write input to buffer
    buffer[writeIndex] = input;
    
    // Get the smoothed delay value for this sample
    double currentDelay = smoothedDelay.getNextValue();
    
    // Calculate the fractional read position
    double readPos = static_cast<double>(writeIndex) - currentDelay;
    
    // Wrap the read position
    while (readPos < 0.0)
        readPos += bufferSize;
    
    // Get integer and fractional parts
    int readIndex1 = static_cast<int>(readPos) % bufferSize;
    int readIndex2 = (readIndex1 + 1) % bufferSize;
    double fraction = readPos - std::floor(readPos);
    
    // Linear interpolation
    float sample1 = buffer[readIndex1];
    float sample2 = buffer[readIndex2];
    float output = sample1 + fraction * (sample2 - sample1);
    
    // Advance write index
    writeIndex = (writeIndex + 1) % bufferSize;
    
    return output;
}

float DelayLine::processSampleNoInterp(int channel, float input)
{
    auto& buffer = buffers[channel];
    auto& writeIndex = writeIndices[channel];
    
    // Write input to buffer
    buffer[writeIndex] = input;
    
    // Get the smoothed delay value (rounded to nearest sample)
    int currentDelay = static_cast<int>(std::round(smoothedDelay.getNextValue()));
    
    // Calculate read index with wrap-around
    int readIndex = (writeIndex - currentDelay + bufferSize) % bufferSize;
    
    // Read the delayed sample
    float output = buffer[readIndex];
    
    // Advance write index
    writeIndex = (writeIndex + 1) % bufferSize;
    
    return output;
}

void DelayLine::processBlock(juce::AudioBuffer<float>& buffer)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels);
    
    // For modulation, delay time might change per sample
    // The linear interpolation in processSample handles this smoothly
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = processSample(ch, channelData[i]);
        }
    }
}



void DelayLine::clear()
{
    for (auto& buffer : buffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    std::fill(writeIndices.begin(), writeIndices.end(), 0);
    
    // Don't reset the delay time - just clear the buffers
}

bool DelayLine::isPrepared() const
{
    return prepared.load();
}

double DelayLine::getMaxDelayTime() const
{
    if (sampleRate <= 0)
        return 0.0;
        
    return maxDelayInSamples / sampleRate;
}

int DelayLine::getMaxDelayInSamples() const
{
    return static_cast<int>(maxDelayInSamples);
}

double DelayLine::getSampleRate() const
{
    return sampleRate;
}

} // namespace audio_plugin