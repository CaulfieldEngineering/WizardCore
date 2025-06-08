#include "DelayLine.h"

namespace audio_plugin {

DelayLine::DelayLine()
    : writeIndex(0)
    , readIndex(0)
    , sampleRate(44100.0)
    , maxDelayInSamples(0)
    , delayInSamples(0)
{
}

DelayLine::~DelayLine() = default;

void DelayLine::prepare(double newSampleRate, double maxDelayTimeInSeconds)
{
    sampleRate = newSampleRate;
    maxDelayInSamples = static_cast<int>(maxDelayTimeInSeconds * sampleRate);
    
    // Create a buffer with 2 channels (stereo) and enough samples for the maximum delay
    buffer.setSize(2, maxDelayInSamples);
    buffer.clear();
    
    writeIndex = 0;
    readIndex = 0;
}

void DelayLine::setDelayTime(double delayTimeInSeconds)
{
    delayInSamples = static_cast<int>(delayTimeInSeconds * sampleRate);
    
    // Ensure delay time doesn't exceed maximum
    delayInSamples = juce::jlimit(0, maxDelayInSamples, delayInSamples);
    
    // Update read index
    readIndex = writeIndex - delayInSamples;
    if (readIndex < 0)
        readIndex += maxDelayInSamples;
}

double DelayLine::getDelayTime() const
{
    return static_cast<double>(delayInSamples) / sampleRate;
}

float DelayLine::process(float input)
{
    // Write the input sample to the buffer
    buffer.setSample(0, writeIndex, input);
    buffer.setSample(1, writeIndex, input);
    
    // Read the delayed sample
    float output = buffer.getSample(0, readIndex);
    
    // Update indices
    writeIndex = (writeIndex + 1) % maxDelayInSamples;
    readIndex = (readIndex + 1) % maxDelayInSamples;
    
    return output;
}

void DelayLine::clear()
{
    buffer.clear();
    writeIndex = 0;
    readIndex = 0;
}

} // namespace audio_plugin
