#include "DigitalDelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

DigitalDelayLine::DigitalDelayLine()
{
}

DigitalDelayLine::~DigitalDelayLine() = default;

void DigitalDelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds, int newNumChannels)
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
    readPositions.resize(numChannels);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffers[ch].resize(bufferSize);
        std::fill(buffers[ch].begin(), buffers[ch].end(), 0.0f);
        writeIndices[ch] = 0;
        readPositions[ch] = 0.0;
    }
    
    // Initialize smoothing
    smoothedDelay.reset(sampleRate, DEFAULT_SMOOTHING_TIME);
    smoothedDelay.setCurrentAndTargetValue(0.0);
    targetDelayInSamples = 0.0;
    
    // Mark as prepared
    prepared.store(true);
}

void DigitalDelayLine::setDelayTime(double delayTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    double delayInSamples = delayTimeInSeconds * sampleRate;
    setDelayInSamples(delayInSamples);
}

void DigitalDelayLine::setDelayInSamples(double delayInSamples)
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

void DigitalDelayLine::setDelayTimeImmediate(double delayTimeInSeconds)
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

void DigitalDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    smoothedDelay.reset(sampleRate, rampTimeInSeconds);
}

double DigitalDelayLine::getDelayTime() const
{
    if (sampleRate <= 0)
        return 0.0;
        
    return targetDelayInSamples / sampleRate;
}

double DigitalDelayLine::getDelayInSamples() const
{
    return targetDelayInSamples;
}

double DigitalDelayLine::getCurrentDelayInSamples() const
{
    return smoothedDelay.getCurrentValue();
}

void DigitalDelayLine::setInterpolationType(InterpolationType type)
{
    interpolationType = type;
}

DigitalDelayLine::InterpolationType DigitalDelayLine::getInterpolationType() const
{
    return interpolationType;
}

float DigitalDelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels);
    
    if (interpolationType == InterpolationType::Linear)
        return processSampleLinearInterp(channel, input);
    else
        return processSampleNoInterp(channel, input);
}

float DigitalDelayLine::processSampleLinearInterp(int channel, float input)
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

float DigitalDelayLine::processSampleNoInterp(int channel, float input)
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



void DigitalDelayLine::processBlock(juce::AudioBuffer<float>& buffer)
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

void DigitalDelayLine::processBlock(juce::AudioBuffer<float>& buffer, float wetMix)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels);
    float dryMix = 1.0f - wetMix;
    
    // For modulation, delay time might change per sample
    // The linear interpolation in processSample handles this smoothly
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



void DigitalDelayLine::clear()
{
    for (auto& buffer : buffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    std::fill(writeIndices.begin(), writeIndices.end(), 0);
    std::fill(readPositions.begin(), readPositions.end(), 0.0);
    
    // Don't reset the delay time - just clear the buffers
}

bool DigitalDelayLine::isPrepared() const
{
    return prepared.load();
}

double DigitalDelayLine::getMaxDelayTime() const
{
    if (sampleRate <= 0)
        return 0.0;
        
    return maxDelayInSamples / sampleRate;
}

int DigitalDelayLine::getMaxDelayInSamples() const
{
    return static_cast<int>(maxDelayInSamples);
}

double DigitalDelayLine::getSampleRate() const
{
    return sampleRate;
}

// Private helper methods
float DigitalDelayLine::getInterpolatedSample(int channel, double readPosition)
{
    auto& buffer = buffers[channel];
    
    // Calculate integer and fractional parts
    int readIndex = static_cast<int>(readPosition);
    double fraction = readPosition - readIndex;
    
    // Handle wrap-around
    readIndex = readIndex % bufferSize;
    if (readIndex < 0) readIndex += bufferSize;
    
    int nextIndex = (readIndex + 1) % bufferSize;
    
    // Linear interpolation
    float sample1 = buffer[readIndex];
    float sample2 = buffer[nextIndex];
    
    return sample1 + (sample2 - sample1) * static_cast<float>(fraction);
}

void DigitalDelayLine::updateReadPositions()
{
    double currentDelay = smoothedDelay.getCurrentValue();
    
    for (int ch = 0; ch < numChannels; ++ch) {
        // Calculate read position relative to write position
        double readPos = static_cast<double>(writeIndices[ch]) - currentDelay;
        
        // Handle wrap-around
        while (readPos < 0) {
            readPos += bufferSize;
        }
        
        readPositions[ch] = readPos;
    }
}

} // namespace audio_plugin