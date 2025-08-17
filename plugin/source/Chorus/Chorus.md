# Chorus Module Documentation

## Overview

The Chorus module provides a sophisticated multi-voice chorus effect using up to 5 modulated delay lines mixed with the dry signal. Each voice has individual parameters for rate, depth, mix, base delay, and phase offset, allowing for rich, complex chorusing effects.

## Future Development

### High Priority
1. **Scale to 10 Voices**
   - Increase MAX_VOICES from 5 to 10
   - Update voice initialization and management
   - Optimize performance for increased voice count
   - Add voice array access methods
   - Consider SIMD optimization for voice processing

2. **Mid/Side Processing Mode**
   - Add Mid/Side encoding/decoding
   - Separate chorus parameters for Mid and Side signals
   - Independent depth control for Mid vs. Side
   - Ability to apply different voice counts to Mid and Side
   - Consider special stereo width enhancement for Side signal

### Core Functionality Enhancements
1. **Signal Path Extensions**
   - Per-voice feedback amount control
   - Cross-voice modulation methods
   - Filter coefficients interface for external filtering
   - Modulation source/destination routing matrix
   - Parallel vs series voice processing modes

2. **Modulation Framework**
   - External modulation input per parameter
   - Modulation smoothing time constants
   - Phase offset matrix between voices
   - Modulation depth scaling curves
   - Host tempo sync interface

3. **Performance Optimizations**
   - SIMD voice processing
   - Optional voice quality settings
   - Automatic voice allocation
   - Dynamic buffer sizing
   - CPU load reporting interface

### Implementation Notes
- Voice scaling requires careful memory management
- Mid/Side processing should be optional with minimal overhead
- Maintain clean public interface for all features
- Keep processing functions real-time safe
- Provide const access methods for parameter inspection
- Consider thread safety for parameter updates
- Document CPU scaling characteristics

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

## Stereo Implementation Options

The current Chorus implementation processes all channels identically, creating a mono chorus effect. Several approaches exist to implement true stereo chorus with left/right channel separation.

### Stereo Implementation Approaches

#### 1. **Stereo Phase Mode** (Implemented)
**What it does:** Applies the same LFO modulation to both channels, but with a time delay (phase offset) between left and right.

**Implementation:**
```cpp
// In Voice struct:
float leftPhaseOffset = 0.0f;
float rightPhaseOffset = 45.0f;  // 45° offset

// In processBlock:
float baseLfoValue = voice.lfo.getNextSample();
float leftLfoValue = baseLfoValue;  // Phase 0°
float rightLfoValue = baseLfoValue + voice.rightPhaseOffset;

// Same modulation depth, same rate, just offset in time
float leftDelay = baseDelay + (leftLfoValue - 0.5f) * 2.0f * modulationRange;
float rightDelay = baseDelay + (rightLfoValue - 0.5f) * 2.0f * modulationRange;
```

**Pros:** Simple to implement, efficient (5 voices = 5 LFOs), immediate stereo separation
**Cons:** Left and right channels are correlated but out of sync
**Audio effect:** Creates a "swirling" stereo image where the chorus effect moves between left and right speakers

### Stereo Control Parameters

#### **Stereo Mode**
- **Mono**: Original mono behavior (default)
- **Stereo**: Stereo phase offset mode with symmetrical left/right processing

#### **Stereo Spread**
- **Range**: 0.0 to 1.0
- **0.0**: Mono (no stereo separation)
- **0.5**: Moderate stereo separation (22.5° phase offset)
- **1.0**: Maximum stereo separation (45° phase offset)

### Usage Examples

#### **Basic Stereo Setup**
```cpp
// Enable stereo mode
chorus.setStereoMode(Chorus::StereoMode::Stereo);
chorus.setStereoSpread(0.8f);  // 80% stereo spread

// Process audio
chorus.processBlock(audioBuffer);
```

#### **Dynamic Stereo Control**
```cpp
// Switch between mono and stereo
chorus.setStereoMode(Chorus::StereoMode::Mono);    // Mono processing
chorus.setStereoMode(Chorus::StereoMode::Stereo);  // Stereo processing

// Adjust stereo width
chorus.setStereoSpread(0.3f);  // Subtle stereo
chorus.setStereoSpread(0.7f);  // Moderate stereo
chorus.setStereoSpread(1.0f);  // Maximum stereo
```

### Technical Implementation

#### **Symmetrical Phase Offsets**
The stereo implementation uses symmetrical phase offsets to avoid lopsided stereo imaging:

```cpp
void Chorus::updateStereoConfiguration() {
    if (currentStereoMode == StereoMode::Stereo) {
        // Calculate symmetrical phase offsets for stereo
        float maxPhaseOffset = stereoSpread * 45.0f;  // 0° to 45° max
        
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& voice = voices[i];
            // Set symmetrical phase offsets to avoid lopsidedness
            voice.leftPhaseOffset = -maxPhaseOffset * 0.5f;   // -22.5° to 0°
            voice.rightPhaseOffset = maxPhaseOffset * 0.5f;   // 0° to +22.5°
        }
    }
}
```

#### **Processing Modes**
The chorus automatically switches between processing modes:

```cpp
void Chorus::processBlock(juce::AudioBuffer<float>& buffer) {
    // Switch between mono and stereo processing based on current mode
    switch (currentStereoMode) {
        case StereoMode::Mono:
            processBlockMono(buffer);      // Original mono processing
            break;
        case StereoMode::Stereo:
            processBlockStereo(buffer);    // New stereo processing
            break;
        default:
            processBlockMono(buffer);      // Fallback to mono
            break;
    }
}
```

### Benefits of the Current Implementation

1. **No Memory Increase**: Reuses existing DelayLines and LFOs
2. **Symmetrical Stereo**: Avoids lopsided stereo imaging
3. **Easy Mode Switching**: Instant switching between mono and stereo
4. **Maintains Quality**: Preserves all existing chorus functionality
5. **Efficient Processing**: Minimal CPU overhead for stereo mode

### Future Enhancements

The current stereo implementation can be extended with:
- **Stereo Spread Mode**: Different modulation depths per channel
- **Independent LFOs**: Completely separate left/right processing
- **Mid-Side Processing**: Process mid and side signals separately
- **Stereo Width Control**: Additional stereo imaging parameters

## Usage Examples

### Basic Multi-Voice Setup
```
```