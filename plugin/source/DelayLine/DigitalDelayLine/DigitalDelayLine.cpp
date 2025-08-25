#include "DigitalDelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

DigitalDelayLine::DigitalDelayLine()
{
    // Initialize with default values
    // All member variables are initialized in their declarations
}

DigitalDelayLine::~DigitalDelayLine() = default;

// ============================================================================
// PREPARATION & SETUP
// ============================================================================

void DigitalDelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds, int newNumChannels)
{
    // Validate input parameters
    jassert(newSampleRate > 0);
    jassert(maxDelayTimeInSeconds > 0);
    jassert(newNumChannels > 0);
    
    // Store system parameters
    sampleRateHz.store(newSampleRate);
    numChannels.store(newNumChannels);
    maxDelayInSamples.store(maxDelayTimeInSeconds * newSampleRate);
    
    // Calculate buffer size with interpolation headroom
    const int calculatedBufferSize = static_cast<int>(std::ceil(maxDelayInSamples.load())) + 2;
    bufferSize.store(calculatedBufferSize);
    
    // Allocate per-channel buffers and indices
    buffers.resize(newNumChannels);
    writeIndices.resize(newNumChannels);
    readPositions.resize(newNumChannels);
    
    // Initialize each channel's buffer and state
    for (int ch = 0; ch < newNumChannels; ++ch)
    {
        buffers[ch].resize(calculatedBufferSize);
        std::fill(buffers[ch].begin(), buffers[ch].end(), 0.0f);
        writeIndices[ch] = 0;
        readPositions[ch] = 0.0;
    }
    
    // Initialize smoothing with current config values
    const double currentSmoothingTime = config.smoothingTimeInSeconds.load();
    smoothedDelay.reset(newSampleRate, currentSmoothingTime);
    smoothedDelay.setCurrentAndTargetValue(0.0);
    targetDelayInSamples.store(0.0);
    
    // Mark as prepared
    prepared.store(true);
}

// ============================================================================
// INDIVIDUAL PARAMETER SETTERS
// ============================================================================

void DigitalDelayLine::setDelayTimeInSeconds(double delayTimeInSeconds, bool withSmoothing)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    // Convert to samples and use the sample-based setter
    const double delayInSamples = delayTimeInSeconds * sampleRateHz.load();
    setDelayInSamples(delayInSamples, withSmoothing);
}

void DigitalDelayLine::setDelayInSamples(double delayInSamples, bool withSmoothing)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    // Clamp to valid range and store in config
    const double clampedDelay = std::clamp(delayInSamples, MIN_DELAY_SAMPLES, maxDelayInSamples.load());
    config.delayInSamples.store(clampedDelay);
    config.delayTimeInSeconds.store(clampedDelay / sampleRateHz.load());
    
    // Update smoothing target
    targetDelayInSamples.store(clampedDelay);
    
    if (withSmoothing)
    {
        // Apply smoothing to prevent clicks
        smoothedDelay.setTargetValue(clampedDelay);
    }
    else
    {
        // Apply immediately without smoothing (may cause clicks)
        smoothedDelay.setCurrentAndTargetValue(clampedDelay);
    }
}

void DigitalDelayLine::setSmoothingTime(double rampTimeInSeconds)
{
    if (!isPrepared())
    {
        jassertfalse;
        return;
    }
    
    // Store in config and update smoothing
    config.smoothingTimeInSeconds.store(rampTimeInSeconds);
    smoothedDelay.reset(sampleRateHz.load(), rampTimeInSeconds);
}

void DigitalDelayLine::setInterpolationType(InterpolationType type)
{
    config.interpolationType.store(type);
}

void DigitalDelayLine::setEnabled(bool enabled)
{
    config.enabled.store(enabled);
}

// ============================================================================
// INDIVIDUAL PARAMETER GETTERS
// ============================================================================

double DigitalDelayLine::getDelayTime() const
{
    if (sampleRateHz.load() <= 0)
        return 0.0;
        
    return config.delayTimeInSeconds.load();
}

double DigitalDelayLine::getDelayInSamples() const
{
    return config.delayInSamples.load();
}

double DigitalDelayLine::getCurrentDelayInSamples() const
{
    return smoothedDelay.getCurrentValue();
}

DigitalDelayLine::InterpolationType DigitalDelayLine::getInterpolationType() const
{
    return config.interpolationType.load();
}

double DigitalDelayLine::getSmoothingTime() const
{
    return config.smoothingTimeInSeconds.load();
}

bool DigitalDelayLine::isEnabled() const
{
    return config.enabled.load();
}

bool DigitalDelayLine::isPrepared() const
{
    return prepared.load();
}

double DigitalDelayLine::getMaxDelayTime() const
{
    if (sampleRateHz.load() <= 0)
        return 0.0;
        
    return maxDelayInSamples.load() / sampleRateHz.load();
}

int DigitalDelayLine::getMaxDelayInSamples() const
{
    return static_cast<int>(maxDelayInSamples.load());
}

double DigitalDelayLine::getSampleRate() const
{
    return sampleRateHz.load();
}

// ============================================================================
// BATCH PARAMETER UPDATES
// ============================================================================

void DigitalDelayLine::updateParameters(std::optional<double> delayTimeInSeconds,
                                       std::optional<double> smoothingTimeInSeconds,
                                       std::optional<InterpolationType> interpolationType,
                                       std::optional<bool> enabled)
{
    // Update delay time if provided
    if (delayTimeInSeconds.has_value())
    {
        setDelayTimeInSeconds(delayTimeInSeconds.value(), true); // Always use smoothing for batch updates
    }
    
    // Update smoothing time if provided
    if (smoothingTimeInSeconds.has_value())
    {
        setSmoothingTime(smoothingTimeInSeconds.value());
    }
    
    // Update interpolation type if provided
    if (interpolationType.has_value())
    {
        setInterpolationType(interpolationType.value());
    }
    
    // Update enabled state if provided
    if (enabled.has_value())
    {
        setEnabled(enabled.value());
    }
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

float DigitalDelayLine::processSample(int channel, float input)
{
    jassert(isPrepared());
    jassert(channel >= 0 && channel < numChannels.load());
    
    // Check if enabled - if not, pass through input unchanged
    if (!config.enabled.load())
    {
        return input;
    }
    
    // Route to appropriate processing method based on interpolation type
    if (config.interpolationType.load() == InterpolationType::Linear)
        return processSampleLinearInterp(channel, input);
    else
        return processSampleNoInterp(channel, input);
}

float DigitalDelayLine::processSampleLinearInterp(int channel, float input)
{
    auto& buffer = buffers[channel];
    auto& writeIndex = writeIndices[channel];
    
    // Write input to buffer at current write position
    buffer[writeIndex] = input;
    
    // Get the smoothed delay value for this sample
    const double currentDelay = smoothedDelay.getNextValue();
    
    // Calculate the fractional read position
    double readPos = static_cast<double>(writeIndex) - currentDelay;
    
    // Handle wrap-around for circular buffer
    while (readPos < 0.0)
        readPos += bufferSize.load();
    
    // Extract integer and fractional parts for interpolation
    const int readIndex1 = static_cast<int>(readPos) % bufferSize.load();
    const int readIndex2 = (readIndex1 + 1) % bufferSize.load();
    const double fraction = readPos - std::floor(readPos);
    
    // Perform linear interpolation
    const float sample1 = buffer[readIndex1];
    const float sample2 = buffer[readIndex2];
    const float output = sample1 + static_cast<float>(fraction * (sample2 - sample1));
    
    // Advance write index with wrap-around
    writeIndex = (writeIndex + 1) % bufferSize.load();
    
    return output;
}

float DigitalDelayLine::processSampleNoInterp(int channel, float input)
{
    auto& buffer = buffers[channel];
    auto& writeIndex = writeIndices[channel];
    
    // Write input to buffer at current write position
    buffer[writeIndex] = input;
    
    // Get the smoothed delay value and round to nearest sample
    const int currentDelay = static_cast<int>(std::round(smoothedDelay.getNextValue()));
    
    // Calculate read index with wrap-around
    const int readIndex = (writeIndex - currentDelay + bufferSize.load()) % bufferSize.load();
    
    // Read the delayed sample
    const float output = buffer[readIndex];
    
    // Advance write index with wrap-around
    writeIndex = (writeIndex + 1) % bufferSize.load();
    
    return output;
}

void DigitalDelayLine::processBlock(juce::AudioBuffer<float>& buffer)
{
    jassert(isPrepared());
    
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(buffer.getNumChannels(), numChannels.load());
    
    // Process each channel independently
    // Linear interpolation in processSample handles smooth modulation
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = processSample(ch, channelData[i]);
        }
    }
}

// ============================================================================
// UTILITY & MAINTENANCE
// ============================================================================

void DigitalDelayLine::clear()
{
    // Clear all delay buffers to silence
    for (auto& buffer : buffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    
    // Reset buffer indices and positions
    std::fill(writeIndices.begin(), writeIndices.end(), 0);
    std::fill(readPositions.begin(), readPositions.end(), 0.0);
    
    // Note: Don't reset the delay time - just clear the buffers
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

float DigitalDelayLine::getInterpolatedSample(int channel, double readPosition)
{
    auto& buffer = buffers[channel];
    
    // Calculate integer and fractional parts
    int readIndex = static_cast<int>(readPosition);
    const double fraction = readPosition - readIndex;
    
    // Handle wrap-around for circular buffer
    readIndex = readIndex % bufferSize.load();
    if (readIndex < 0) 
        readIndex += bufferSize.load();
    
    const int nextIndex = (readIndex + 1) % bufferSize.load();
    
    // Perform linear interpolation
    const float sample1 = buffer[readIndex];
    const float sample2 = buffer[nextIndex];
    
    return sample1 + static_cast<float>((sample2 - sample1) * fraction);
}

void DigitalDelayLine::updateReadPositions()
{
    const double currentDelay = smoothedDelay.getCurrentValue();
    
    // Update read positions for all channels
    for (int ch = 0; ch < numChannels.load(); ++ch) 
    {
        // Calculate read position relative to write position
        double readPos = static_cast<double>(writeIndices[ch]) - currentDelay;
        
        // Handle wrap-around
        while (readPos < 0) 
        {
            readPos += bufferSize.load();
        }
        
        readPositions[ch] = readPos;
    }
}

} // namespace audio_plugin
