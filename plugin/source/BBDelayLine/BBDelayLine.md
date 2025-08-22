# BBDelayLine - Bucket Brigade Delay Emulation

## Overview

The **BBDelayLine** is a specialized delay processor that emulates the characteristic sound of analog Bucket Brigade Delay (BBD) circuits. It inherits from the base `DelayLine` class, ensuring full interface compatibility while adding authentic BBD characteristics.

BBD circuits were commonly used in vintage delay pedals, synthesizers, and audio equipment from the 1970s and 1980s. They create a warm, slightly degraded sound with characteristic artifacts that many musicians and producers find desirable for certain musical styles.

### Key Features

- **Full DelayLine Compatibility**: Inherits all base functionality while adding BBD characteristics
- **Authentic BBD Emulation**: Simulates clock noise, bandwidth limitations, and sample rate conversion artifacts
- **Configurable Character**: Three preset modes (Vintage, Modern, Dirty) plus fine-tuned control
- **Clock Rate Control**: Adjustable internal clock rate for different BBD circuit types
- **Clock Modulation**: Subtle clock rate variations for chorus/flanger effects
- **Thread-Safe**: Inherits thread safety from base DelayLine class
- **Memory Efficient**: Minimal additional overhead beyond base DelayLine

### BBD Circuit Characteristics

Bucket Brigade Delay circuits work by:
1. **Sampling** audio at a lower internal clock rate (typically 1-10kHz)
2. **Storing** samples in a chain of capacitors
3. **Transferring** samples through the chain using clock signals
4. **Reconstructing** the delayed signal with inherent artifacts

The BBDelayLine emulates these characteristics:
- **Clock Noise**: Simulates clock jitter and switching artifacts
- **Bandwidth Reduction**: Emulates the limited frequency response of BBD circuits
- **Sample Rate Conversion**: Creates subtle artifacts from internal clock rate differences
- **Warm Character**: Adds subtle harmonic distortion and filtering

## Comprehensive Parameter Table

| Parameter | Type | Range/Options | Default | Description | Thread Safe |
|-----------|------|---------------|---------|-------------|-------------|
| **All DelayLine Parameters** | Various | See DelayLine docs | See DelayLine docs | Inherited from base class | ✅ |
| **Clock Rate** | `double` | 100-50,000 Hz | 2,000 Hz | Internal BBD clock rate | ✅ |
| **Noise Amount** | `double` | 0.0 to 1.0 | 0.15 | Clock noise and jitter | ✅ |
| **Bandwidth Reduction** | `double` | 0.0 to 1.0 | 0.3 | High-frequency filtering | ✅ |
| **BBD Characteristic** | `BBDCharacteristic` | Vintage/Modern/Dirty | Vintage | Preset configuration | ✅ |
| **Clock Modulation Depth** | `double` | 0.0 to 1.0 | 0.0 | Clock rate modulation amount | ✅ |
| **Clock Modulation Rate** | `double` | 0.0 to 20.0 Hz | 0.0 | Clock rate modulation frequency | ✅ |

### BBDCharacteristic Enum

| Value | Description | Noise Amount | Bandwidth Reduction | Use Cases |
|-------|-------------|--------------|---------------------|-----------|
| `Vintage` | Classic BBD sound | 0.15 (15%) | 0.3 (30%) | Traditional analog delay, vintage character |
| `Modern` | Cleaner BBD with subtle artifacts | 0.05 (5%) | 0.15 (15%) | Modern productions, subtle warmth |
| `Dirty` | Heavy degradation and noise | 0.4 (40%) | 0.6 (60%) | Lo-fi effects, experimental music |

## Usage Examples

### Basic BBD Delay Setup

```cpp
BBDelayLine bbDelay;

// Prepare for 48kHz, 1 second max delay, stereo
bbDelay.prepare(48000.0, 1.0, 2);

// Set delay time to 300ms
bbDelay.setDelayTime(0.3);

// Configure BBD characteristics
bbDelay.setClockRate(1500);        // 1.5kHz internal clock
bbDelay.setNoiseAmount(0.2);       // 20% noise
bbDelay.setBandwidthReduction(0.4); // 40% bandwidth reduction

// Process audio
bbDelay.processBlock(audioBuffer);
```

### Preset Character Selection

```cpp
// Use vintage preset for classic sound
bbDelay.setBBDCharacteristic(BBDelayLine::BBDCharacteristic::Vintage);

// Or use modern for cleaner sound
bbDelay.setBBDCharacteristic(BBDelayLine::BBDCharacteristic::Modern);

// Or dirty for lo-fi effects
bbDelay.setBBDCharacteristic(BBDelayLine::BBDCharacteristic::Dirty);
```

### Clock Modulation for Chorus/Flanger Effects

```cpp
// Add subtle clock rate modulation
bbDelay.setClockModulation(0.3, 2.0); // 30% depth, 2Hz rate

// This creates a subtle chorus-like effect by varying the delay time
// through clock rate changes, similar to how real BBD circuits behave
```

### Seamless Switching Between Delay Types

```cpp
// Since BBDelayLine inherits from DelayLine, you can easily switch:
DelayLine* currentDelay = nullptr;

if (useBBD)
{
    static BBDelayLine bbDelay;
    currentDelay = &bbDelay;
    bbDelay.setBBDCharacteristic(BBDelayLine::BBDCharacteristic::Vintage);
}
else
{
    static DelayLine cleanDelay;
    currentDelay = &cleanDelay;
}

// Both have identical interfaces, so switching is seamless
currentDelay->prepare(48000.0, 1.0, 2);
currentDelay->setDelayTime(0.25);
currentDelay->processBlock(audioBuffer);
```

## Technical Implementation Details

### BBD Processing Pipeline

1. **Input Processing**: Audio is first processed through BBD simulation
2. **Clock Simulation**: Internal clock rate determines processing frequency
3. **Noise Addition**: Clock jitter and noise are added based on settings
4. **Bandwidth Filtering**: Low-pass filtering simulates BBD frequency limitations
5. **Base Delay**: Processed audio then goes through standard DelayLine processing

### Clock Rate Impact

- **Lower Clock Rates** (1-2kHz): More artifacts, vintage character, lower CPU
- **Higher Clock Rates** (5-10kHz): Cleaner sound, less artifacts, higher CPU
- **Typical Range**: 1.5-3kHz for most vintage BBD emulations

### Memory Usage

BBDelayLine adds minimal memory overhead:
- **Per Channel**: ~2-4 additional float buffers (depending on clock rate)
- **Filter State**: 2 float values per channel
- **Total Overhead**: Typically <1% of base DelayLine memory usage

## Performance Considerations

### CPU Usage
- **Base Processing**: Same as DelayLine
- **BBD Simulation**: Adds ~5-15% CPU overhead depending on settings
- **Clock Rate Impact**: Higher clock rates increase CPU usage
- **Noise Amount**: Higher noise amounts slightly increase CPU usage

### Memory Usage
- **Base Memory**: Same as DelayLine
- **BBD Buffers**: Proportional to clock rate and delay time
- **Filter State**: Minimal per-channel overhead

### Optimization Tips
1. **Use appropriate clock rates** for your application
2. **Disable clock modulation** if not needed
3. **Choose preset characteristics** rather than fine-tuning for performance
4. **Consider noise amount** - higher values increase CPU usage

## Common Applications

### Vintage Delay Effects
- **Echo**: 100-500ms delays with vintage character
- **Slapback**: 50-150ms delays for rockabilly/rock sounds
- **Doubling**: 10-30ms delays for width without obvious delay

### Lo-Fi and Experimental
- **Degraded Delays**: Heavy noise and filtering for experimental music
- **Vintage Synth**: Emulate classic analog synthesizer delay sections
- **Tape-like Effects**: Combine with other effects for tape machine emulation

### Chorus and Flanger
- **Clock Modulation**: Use clock rate modulation for movement
- **Short Delays**: 10-50ms delays with BBD character
- **Stereo Effects**: Different settings per channel for width

## Troubleshooting

### Common Issues

**No BBD Character**: Ensure `noiseAmount` and `bandwidthReduction` are > 0
**Too Much Noise**: Reduce `noiseAmount` or use `Modern` characteristic
**Excessive Filtering**: Reduce `bandwidthReduction` or use `Modern` characteristic
**High CPU Usage**: Reduce `clockRate` or disable clock modulation

### Performance Monitoring

Monitor these parameters for optimal performance:
- Clock rate vs. desired character
- Noise amount vs. acceptable artifacts
- Bandwidth reduction vs. frequency response needs

## Future Enhancements

Potential future additions to BBDelayLine:
- **Temperature Simulation**: Emulate BBD circuit temperature variations
- **Power Supply Noise**: Simulate power supply ripple effects
- **Component Aging**: Emulate circuit component degradation over time
- **Advanced Filtering**: More sophisticated bandwidth simulation algorithms
