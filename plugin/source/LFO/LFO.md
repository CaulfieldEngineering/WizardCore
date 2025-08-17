# LFO (Low Frequency Oscillator)

## Overview
This module provides a thread-safe, phase-accurate sine wave LFO (Low Frequency Oscillator) for audio modulation. The LFO is designed to generate smooth modulation signals suitable for modulating various audio parameters such as delay time, filter cutoff, and other time-varying effects.

## Features

### Core Functionality
- **Sine Wave Generation**: High-quality sine wave output using phase accumulation
- **Phase-Accurate**: Maintains precise phase relationships and frequency control
- **Thread-Safe**: Safe for real-time audio processing
- **Real-Time Control**: Supports parameter changes during playback without discontinuities
- **Minimal Memory Footprint**: No dynamic allocation after initialization

### Parameters
- **Frequency**: 0.1 Hz to 20 Hz range (suitable for most modulation applications)
- **Depth**: 0.0 to 1.0 modulation depth control
- **Phase Offset**: 0 to 2π radians for phase synchronization
- **Waveform Type**: Currently supports sine wave (extensible for future waveforms)

## Implementation Details

### Architecture
The LFO uses a phase accumulator approach for precise frequency control:
- **Phase Accumulator**: Maintains the current phase position
- **Phase Increment**: Calculated as `(2π × frequency) / sampleRate`
- **Phase Wrapping**: Ensures phase stays within [0, 2π) range
- **Depth Scaling**: Applies modulation depth to the output signal

### Thread Safety
- Uses `std::atomic<bool>` for prepared state
- All parameter access is thread-safe for audio processing
- No locks or mutexes required for normal operation

### Performance
- **CPU**: Minimal computational overhead (~1-2 operations per sample)
- **Memory**: Static allocation after prepare() call
- **Latency**: Zero samples of latency

## Usage Examples

### Basic LFO Setup
```cpp
#include "LFO/LFO.h"

// Create and configure LFO
audio_plugin::LFO lfo;
lfo.prepare(48000.0);        // 48kHz sample rate
lfo.setFrequency(2.0);       // 2 Hz sine wave
lfo.setDepth(0.5f);          // 50% modulation depth
```

### Audio Processing Integration
```cpp
// In your processBlock method:
for (int sample = 0; sample < numSamples; ++sample)
{
    // Get LFO modulation value
    float lfoValue = lfo.getNextSample();
    
    // Apply modulation to your parameter
    float modulatedDelay = baseDelay + (lfoValue * modulationRange);
    delayLine.setDelayTime(modulatedDelay);
    
    // Process audio...
}
```

### Chorus/Flanger Effect
```cpp
// Configure LFO for chorus effect
lfo.setFrequency(1.5);       // 1.5 Hz for slow modulation
lfo.setDepth(0.3f);          // 30% depth for subtle effect

// In processBlock:
float lfoValue = lfo.getNextSample();
float delayMs = 20.0f + (lfoValue * 10.0f);  // 20ms ± 10ms
delayLine.setDelayTime(delayMs * 0.001f);
```

### Phase Synchronization
```cpp
// Create two LFOs with phase relationship
audio_plugin::LFO lfo1, lfo2;
lfo1.prepare(48000.0);
lfo2.prepare(48000.0);

lfo1.setFrequency(2.0);
lfo2.setFrequency(2.0);
lfo2.setPhaseOffset(M_PI);  // 180° phase offset
```

## Integration with Plugin Processor

### Adding LFO to PluginProcessor.h
```cpp
#include "LFO/LFO.h"

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
private:
    // Add LFO instance
    LFO lfo;
    
    // Add parameter pointers
    std::atomic<float>* lfoFrequencyParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* lfoEnabledParam = nullptr;
};
```

### Adding LFO Parameters
```cpp
// In constructor:
std::make_unique<juce::AudioParameterFloat>(
    "lfoFrequency", 
    "LFO Frequency", 
    juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f),
    1.0f),  // default 1 Hz
std::make_unique<juce::AudioParameterFloat>(
    "lfoDepth",
    "LFO Depth",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
    0.5f),  // default 50%
std::make_unique<juce::AudioParameterBool>(
    "lfoEnabled",
    "LFO Enabled",
    false)  // default disabled
```

### Preparing LFO in prepareToPlay
```cpp
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare delay line
    delayLine.prepare(sampleRate, 2.0, getTotalNumOutputChannels());
    
    // Prepare LFO
    lfo.prepare(sampleRate);
    
    // Get parameter pointers
    lfoFrequencyParam = parameters.getRawParameterValue("lfoFrequency");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    lfoEnabledParam = parameters.getRawParameterValue("lfoEnabled");
}
```

### Using LFO in processBlock
```cpp
void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Update LFO parameters
    if (lfoEnabledParam->load())
    {
        lfo.setFrequency(lfoFrequencyParam->load());
        lfo.setDepth(lfoDepthParam->load());
        
        // Apply LFO modulation to delay time
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float lfoValue = lfo.getNextSample();
            float baseDelay = delayTimeParam->load();
            float modulatedDelay = baseDelay + (lfoValue * 0.1f);  // ±100ms modulation
            delayLine.setDelayTime(modulatedDelay);
        }
    }
    
    // Process audio...
}
```

## Future Enhancements

### Planned Waveforms
- **Triangle**: Linear ramp up/down
- **Square**: Binary on/off modulation
- **Saw**: Linear ramp with reset
- **Random**: Sample-and-hold random values
- **Custom**: User-defined waveform shapes

### Advanced Features
- **Sync to Host**: Tempo-synchronized LFO rates
- **MIDI Sync**: External MIDI clock synchronization
- **Envelope Follower**: Audio-reactive modulation
- **Multiple LFOs**: Independent LFO instances
- **Modulation Matrix**: Flexible routing system

## Technical Notes

### Frequency Range
- **Minimum**: 0.1 Hz (10-second cycle)
- **Maximum**: 20 Hz (50ms cycle)
- **Resolution**: Phase-accurate to sample rate

### Output Range
- **Raw Output**: [-1.0, +1.0] before depth scaling
- **Scaled Output**: [-depth, +depth] after depth scaling
- **DC Offset**: Centered at 0.0

### Phase Handling
- **Wrapping**: Automatic phase wrapping to [0, 2π)
- **Precision**: Double-precision phase accumulation
- **Continuity**: Smooth transitions during parameter changes

## Open Issues
- Add remaining waveforms
- Parameter smoothing needs optimization (slider is faster than smoothing values)

## Dependencies
- **JUCE**: Audio processing framework
- **C++17**: Standard library features (std::clamp, etc.)
- **Standard Math**: std::sin, std::cos for waveform generation 