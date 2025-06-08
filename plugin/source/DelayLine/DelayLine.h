#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace audio_plugin {

class DelayLine {
public:
    DelayLine();
    ~DelayLine();

    // Initialize the delay line with a maximum delay time
    void prepare(double sampleRate, double maxDelayTimeInSeconds);

    // Set the current delay time in seconds
    void setDelayTime(double delayTimeInSeconds);

    // Get the current delay time in seconds
    double getDelayTime() const;

    // Process a single sample
    float process(float input);

    // Clear the delay line
    void clear();

private:
    juce::AudioBuffer<float> buffer;
    int writeIndex;
    int readIndex;
    double sampleRate;
    int maxDelayInSamples;
    int delayInSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayLine)
};

} // namespace audio_plugin
