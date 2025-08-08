# DelayLine (Interpolating Delay Line)

## Overview
This module provides a thread-safe, interpolating delay line for audio processing. The DelayLine class offers a circular buffer-based delay implementation with linear interpolation for smooth delay time changes and modulation. It supports multi-channel processing with independent delay buffers per channel, making it suitable for stereo effects, chorus, flanger, and other time-based audio effects.

## Features

### Core Functionality
- **Multi-Channel Support**: Independent delay buffers for each audio channel
- **Linear Interpolation**: Smooth delay time changes without zipper noise
- **Circular Buffer**: Efficient memory usage with automatic buffer wrapping
- **Thread-Safe**: Safe for parallel channel processing
- **Real-Time Modulation**: Supports audio-rate delay time changes
- **Smoothing Control**: Configurable ramp time for parameter changes

### Parameters
- **Delay Time**: 0 to maximum delay time in seconds
- **Interpolation Type**: Linear interpolation or nearest sample
- **Smoothing Time**: Configurable ramp time for delay changes
- **Channel Count**: Automatic multi-channel support
- **Sample Rate**: Automatic adaptation to different sample rates

## Implementation Details

### Architecture
The DelayLine uses a circular buffer approach with interpolation:
- **Per-Channel Buffers**: Independent circular buffers for each channel
- **Write Indices**: Separate write positions for each channel
- **Interpolation**: Linear interpolation between samples for fractional delays
- **Smoothing**: JUCE SmoothedValue for parameter changes
- **Memory Management**: Efficient allocation with interpolation headroom

### Thread Safety
- Uses `std::atomic<bool>` for prepared state
- Independent buffers per channel for parallel processing
- Thread-safe parameter updates with smoothing
- No locks required for normal audio processing

### Performance
- **CPU**: Minimal overhead with efficient circular buffer access
- **Memory**: Allocates `(maxDelaySeconds × sampleRate × numChannels × sizeof(float))` bytes
- **Latency**: Configurable delay time (0 to maximum)
- **Interpolation**: Linear interpolation adds minimal computational cost

## Usage Examples

### Basic Delay Setup
```cpp
#include "DelayLine/DelayLine.h"

// Create and configure delay line
audio_plugin::DelayLine delay;
delay.prepare(48000.0, 2.0, 2);  // 48kHz, 2 seconds max, stereo
delay.setDelayTime(0.25);         // 250ms delay
delay.setInterpolationType(audio_plugin::DelayLine::InterpolationType::Linear);
```

### Simple Delay Effect
```cpp
// In your processBlock method:
void processBlock(juce::AudioBuffer<float>& buffer)
{
    // Process each sample
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float input = buffer.getSample(channel, sample);
            float delayed = delay.processSample(channel, input);
            buffer.setSample(channel, sample, delayed);
        }
    }
}
```

### Dry/Wet Mix Processing
```cpp
// Process with mix control
delay.processBlock(audioBuffer, 0.5f);  // 50% wet mix
```

### Modulated Delay (Chorus/Flanger)
```cpp
// Configure for chorus effect
delay.prepare(48000.0, 0.1, 2);  // 100ms max delay for chorus
delay.setDelayTime(0.02);         // 20ms base delay

// In processBlock with LFO modulation:
for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
{
    float lfoValue = lfo.getNextSample();
    float delayMs = 20.0f + (lfoValue * 10.0f);  // 20ms ± 10ms
    delay.setDelayTime(delayMs * 0.001f);
    
    // Process audio...
}
```

### Stereo Delay with Different Times
```cpp
// For stereo delay with different times per channel:
delay.setDelayTime(0.3);  // 300ms for left channel
// Note: Currently uses same delay time for all channels
// Future enhancement: per-channel delay times
```

## Integration with Plugin Processor

### Adding DelayLine to PluginProcessor.h
```cpp
#include "DelayLine/DelayLine.h"

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
private:
    // Delay line instance
    DelayLine delayLine;
    
    // Parameter pointers for quick access
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    
    // Smoothed value for mix parameter
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
};
```

### Adding DelayLine Parameters
```cpp
// In constructor:
std::make_unique<juce::AudioParameterFloat>(
    "delayTime", 
    "Delay Time", 
    juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f),  // 0-2 seconds with 1ms steps
    0.25f),  // default 250ms
std::make_unique<juce::AudioParameterFloat>(
    "mix",
    "Mix",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
    0.5f)  // default 50% wet
```

### Preparing DelayLine in prepareToPlay
```cpp
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare the delay line with max 2 seconds of delay
    delayLine.prepare(sampleRate, 2.0, getTotalNumOutputChannels());
    
    // Set interpolation type
    delayLine.setInterpolationType(DelayLine::InterpolationType::Linear);
    
    // Set smoothing time for delay changes
    delayLine.setSmoothingTime(0.05);  // 50ms smoothing
    
    // Get parameter pointers
    delayTimeParam = parameters.getRawParameterValue("delayTime");
    mixParam = parameters.getRawParameterValue("mix");
    
    // Prepare smoothed mix value
    smoothedMix.reset(sampleRate, 0.05);
}
```

### Using DelayLine in processBlock
```cpp
void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Update delay time from parameter
    float delayTime = delayTimeParam->load();
    delayLine.setDelayTime(delayTime);
    
    // Update mix parameter with smoothing
    float mix = mixParam->load();
    smoothedMix.setTargetValue(mix);
    
    // Process audio with dry/wet mix
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float currentMix = smoothedMix.getNextValue();
        
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float input = buffer.getSample(channel, sample);
            float delayed = delayLine.processSample(channel, input);
            
            // Apply dry/wet mix
            float output = (input * (1.0f - currentMix)) + (delayed * currentMix);
            buffer.setSample(channel, sample, output);
        }
    }
}
```

## Advanced Usage

### Chorus Effect Implementation
```cpp
// Configure for chorus
delayLine.prepare(48000.0, 0.1, 2);  // 100ms max delay
delayLine.setDelayTime(0.02);         // 20ms base delay

// In processBlock with LFO:
for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
{
    float lfoValue = lfo.getNextSample();
    float delayMs = 20.0f + (lfoValue * 5.0f);  // 20ms ± 5ms
    delayLine.setDelayTime(delayMs * 0.001f);
    
    // Process with high wet mix for chorus
    delayLine.processBlock(buffer, 0.7f);
}
```

### Flanger Effect Implementation
```cpp
// Configure for flanger
delayLine.prepare(48000.0, 0.02, 2);  // 20ms max delay
delayLine.setDelayTime(0.005);         // 5ms base delay

// In processBlock with faster LFO:
for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
{
    float lfoValue = lfo.getNextSample();
    float delayMs = 5.0f + (lfoValue * 3.0f);  // 5ms ± 3ms
    delayLine.setDelayTime(delayMs * 0.001f);
    
    // Process with moderate wet mix for flanger
    delayLine.processBlock(buffer, 0.5f);
}
```

### Echo/Delay Chain
```cpp
// Multiple delay lines for complex effects
DelayLine delay1, delay2, delay3;

delay1.prepare(48000.0, 1.0, 2);
delay2.prepare(48000.0, 1.0, 2);
delay3.prepare(48000.0, 1.0, 2);

delay1.setDelayTime(0.25);  // 250ms
delay2.setDelayTime(0.5);   // 500ms
delay3.setDelayTime(0.75);  // 750ms

// Process in series
delay1.processBlock(buffer, 0.3f);
delay2.processBlock(buffer, 0.2f);
delay3.processBlock(buffer, 0.1f);
```

## Future Enhancements

### Planned Features
- **Per-Channel Delay Times**: Independent delay times for each channel
- **Feedback Control**: Built-in feedback parameter for echo effects
- **Filter Integration**: Low-pass/high-pass filters in feedback path
- **Tempo Sync**: Host tempo synchronization for delay times
- **MIDI Sync**: External MIDI clock synchronization
- **Tap Tempo**: Manual tempo tapping for delay times

### Advanced Interpolation
- **Cubic Interpolation**: Higher quality interpolation for critical applications
- **All-Pass Interpolation**: Phase-correct interpolation
- **Variable Interpolation**: Adaptive interpolation based on frequency content

### Modulation Features
- **Envelope Follower**: Audio-reactive delay time modulation
- **Sidechain Modulation**: External signal modulation
- **Random Modulation**: Stochastic delay time variations

## Technical Notes

### Delay Time Range
- **Minimum**: 0.0 seconds (no delay)
- **Maximum**: Configurable up to several seconds
- **Resolution**: Sample-accurate with interpolation
- **Smoothing**: Configurable ramp time for changes

### Memory Usage
- **Per Channel**: `maxDelaySeconds × sampleRate × sizeof(float)` bytes
- **Interpolation Headroom**: +2 samples for linear interpolation
- **Total Memory**: `numChannels × (bufferSize + 2) × sizeof(float)` bytes

### Performance Characteristics
- **CPU Usage**: Minimal overhead for circular buffer access
- **Memory Access**: Cache-friendly sequential access patterns
- **Interpolation Cost**: ~2-3 additional operations per sample
- **Smoothing Cost**: JUCE SmoothedValue overhead

### Quality Considerations
- **Aliasing**: No aliasing issues with delay time changes
- **Phase Continuity**: Smooth phase transitions during modulation
- **Frequency Response**: Flat response across audio spectrum
- **Noise Floor**: No additional noise introduced

## Dependencies
- **JUCE**: Audio processing framework and SmoothedValue
- **C++17**: Standard library features (std::clamp, std::ceil, etc.)
- **Standard Math**: std::ceil for buffer size calculation
- **Standard Algorithm**: std::fill for buffer initialization

