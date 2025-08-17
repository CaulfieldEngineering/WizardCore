# Chorus Module Documentation

## Overview

The Chorus module provides a sophisticated multi-voice chorus effect using up to 5 modulated delay lines mixed with the dry signal. Each voice has individual parameters for rate, depth, mix, base delay, and phase offset, allowing for rich, complex chorusing effects.

## Architecture

### Core Components

1. **Multiple LFOs (Low Frequency Oscillators)**: Each voice has its own LFO for independent modulation
2. **Multiple DelayLines**: Each voice has its own delay line for independent processing
3. **Voice Management**: Individual control over each voice's parameters and enable/disable state
4. **Mix Control**: Global and per-voice balance between dry and wet signals

### Signal Flow

```
Input → Split → [Voice 0: DelayLine + LFO] → Mix → 
         ↓      [Voice 1: DelayLine + LFO] → Mix → 
      Dry Signal [Voice 2: DelayLine + LFO] → Mix → Output
                 [Voice 3: DelayLine + LFO] → Mix → 
                 [Voice 4: DelayLine + LFO] → Mix → 
```

### How It Works

1. **Input Signal**: The audio input is duplicated for each enabled voice
2. **Independent Modulation**: Each voice has its own LFO modulating its delay time
3. **Phase Relationships**: Voices can have different phase offsets for complex interactions
4. **Individual Processing**: Each voice processes independently with its own parameters
5. **Combined Output**: All voice outputs are mixed together and then with the dry signal

## Parameters

### Global Parameters (affect all enabled voices)

#### Rate (0.1 - 2.0 Hz)
- Controls how fast all enabled voices modulate
- Lower values (0.1-0.5 Hz) create slow, musical chorusing
- Higher values (1.0-2.0 Hz) create faster, more pronounced effects
- Typical chorus uses 0.5-1.0 Hz

#### Depth (0.0 - 1.0)
- Controls how much all enabled voices modulate their delay times
- 0.0 = no modulation (static delay)
- 1.0 = maximum modulation (±5ms around base delay)
- Typical chorus uses 0.3-0.7

#### Mix (0.0 - 1.0)
- Controls the global balance between dry and wet signals
- 0.0 = 100% dry (no effect)
- 0.5 = 50/50 mix
- 1.0 = 100% wet (effect only)
- Typical chorus uses 0.6-0.8

#### Base Delay (10 - 100 ms)
- Sets the center delay time for all enabled voices
- Lower values (10-20ms) create subtle effects
- Higher values (30-50ms) create more pronounced chorusing
- Typical chorus uses 20-40ms

### Per-Voice Parameters

#### Voice Enable/Disable
- Each voice (0-4) can be independently enabled or disabled
- Allows for dynamic voice count and performance control

#### Individual Rate (0.1 - 2.0 Hz)
- Each voice can have its own modulation rate
- Creates complex, evolving chorusing patterns
- Different rates between voices add richness

#### Individual Depth (0.0 - 1.0)
- Each voice can have its own modulation depth
- Allows for subtle to pronounced effects per voice
- Creates dynamic, layered chorusing

#### Individual Mix (0.0 - 1.0)
- Each voice can have its own wet/dry balance
- Enables fine control over voice contribution
- Useful for creating depth and space

#### Individual Base Delay (10 - 100 ms)
- Each voice can have its own center delay time
- Creates stereo spread and depth
- Different delays between voices enhance the chorus effect

#### Phase Offset (0° - 360°)
- Each voice can have its own phase offset
- Creates complex interactions between voices
- 72° spacing (360°/5) provides good voice separation

## Technical Details

### LFO Configuration
Each voice's LFO is configured independently:
- **Waveform**: Sine wave (smooth, musical modulation)
- **Symmetry**: 50% (symmetric waveform)
- **Phase Offset**: Individual per voice (0° to 360°)
- **Inversion**: Disabled
- **Frequency**: Individual per voice

### Delay Line Configuration
Each voice has its own delay line:
- **Interpolation**: Linear interpolation for smooth modulation
- **Maximum Delay**: Automatically calculated per voice
- **Channels**: Supports any number of channels (mono, stereo, etc.)

### Performance Characteristics
- **CPU Usage**: Moderate (5 LFOs + 5 delay lines when all voices enabled)
- **Memory**: Moderate (5x delay buffers + 5x LFO wavetables)
- **Latency**: Equal to maximum delay time across all voices
- **Thread Safety**: Fully thread-safe for audio processing

## Usage Examples

### Basic Multi-Voice Setup
```cpp
Chorus chorus;
chorus.prepare(48000, 2);  // 48kHz, stereo

// Enable and configure individual voices
chorus.setVoiceEnabled(0, true);
chorus.setVoiceRate(0, 0.8f);       // 0.8 Hz modulation
chorus.setVoiceDepth(0, 0.5f);      // 50% modulation depth
chorus.setVoiceMix(0, 0.7f);        // 70% wet, 30% dry
chorus.setVoiceBaseDelay(0, 30.0f); // 30ms base delay
chorus.setVoicePhaseOffset(0, 0.0f); // No phase offset

// Enable second voice with different parameters
chorus.setVoiceEnabled(1, true);
chorus.setVoiceRate(1, 1.2f);       // 1.2 Hz modulation
chorus.setVoiceDepth(1, 0.4f);      // 40% modulation depth
chorus.setVoiceMix(1, 0.6f);        // 60% wet, 40% dry
chorus.setVoiceBaseDelay(1, 35.0f); // 35ms base delay
chorus.setVoicePhaseOffset(1, 72.0f); // 72° phase offset
```

### Global Parameter Control
```cpp
// Set all enabled voices to same rate
chorus.setRate(1.0f);

// Set all enabled voices to same depth
chorus.setDepth(0.6f);

// Set global mix (affects final output)
chorus.setMix(0.8f);
```

### Processing Audio
```cpp
// In your processBlock function:
chorus.processBlock(audioBuffer);
```

### Dynamic Voice Management
```cpp
// Enable/disable voices dynamically
chorus.setVoiceEnabled(2, true);   // Enable voice 2
chorus.setVoiceEnabled(3, false);  // Disable voice 3

// Check voice status
bool voice0Enabled = chorus.isVoiceEnabled(0);
```

## Integration Notes

### With PluginProcessor
The multi-voice Chorus module integrates seamlessly with your existing PluginProcessor:
- No modification of DelayLine or LFO classes required
- Follows the same parameter validation and error handling patterns
- Uses the same threading model and safety features
- Provides both global and per-voice parameter control

### Parameter Ranges
All parameters are automatically clamped to their valid ranges:
- **Rate**: 0.1 Hz to 2.0 Hz
- **Depth**: 0.0 to 1.0
- **Mix**: 0.0 to 1.0
- **Base Delay**: 10ms to 100ms
- **Phase Offset**: 0° to 360°

### Error Handling
The module includes comprehensive error checking:
- Invalid voice indices are rejected with debug output
- Invalid parameters are rejected with debug output
- Processing is skipped if not properly prepared
- Graceful degradation when parameters are out of range

## Advanced Usage Patterns

### Stereo Spread
```cpp
// Create stereo spread with different delays
chorus.setVoiceBaseDelay(0, 25.0f); // Left voice
chorus.setVoiceBaseDelay(1, 35.0f); // Right voice
chorus.setVoicePhaseOffset(0, 0.0f);   // Left phase
chorus.setVoicePhaseOffset(1, 180.0f); // Right phase (opposite)
```

### Frequency Separation
```cpp
// Different rates for different frequency ranges
chorus.setVoiceRate(0, 0.5f);  // Slow, deep modulation
chorus.setVoiceRate(1, 1.5f);  // Fast, bright modulation
```

### Dynamic Voice Count
```cpp
// Start with one voice, add more for intensity
chorus.setVoiceEnabled(0, true);
chorus.setVoiceEnabled(1, false);
chorus.setVoiceEnabled(2, false);
// ... enable more voices as needed
```

## Future Enhancements

This multi-voice implementation can be further expanded to include:
- Different LFO waveforms per voice
- Voice-specific feedback control
- More sophisticated modulation patterns
- Preset management for voice combinations
- Real-time voice morphing
- Spectral analysis for adaptive parameters

## Dependencies

- **JUCE Audio Processors**: For audio buffer handling
- **LFO Module**: For modulation signal generation (5 instances)
- **DelayLine Module**: For time-varying delays (5 instances)
- **Standard C++**: For atomic operations, containers, and arrays
