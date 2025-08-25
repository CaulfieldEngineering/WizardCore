# DelayLine - Base Class Architecture

## Overview

The `DelayLine` class serves as the foundational architecture for all delay line implementations in the WizardOne system. It establishes a unified interface and common infrastructure that enables different delay technologies to coexist while maintaining consistent behavior and parameter management.

## Design Philosophy

### Polymorphic Architecture
The DelayLine system employs a polymorphic design that separates interface from implementation. This approach allows different delay technologies (digital, BBD, analog emulation) to share common functionality while preserving their unique sonic characteristics.

### Config Struct Pattern
Following the established design rule, all sonic parameters are centralized in a `Config` struct that contains only parameters of musical or sonic interest. This provides a clear separation between creative parameters and technical implementation details.

### Thread-Safe Parameter Management
All sonic parameters use atomic operations to ensure thread-safe access during real-time audio processing. This enables parameter modulation and automation without audio glitches or race conditions.

## Theory of Operation

### Core Delay Mechanism
At its heart, a delay line is a time-domain signal processor that stores audio samples in a circular buffer and reads them back after a specified time interval. The fundamental equation governing delay operation is:

```
y[n] = x[n] + g * x[n - D]
```

Where:
- `y[n]` is the output sample at time n
- `x[n]` is the input sample at time n
- `g` is the feedback/gain factor
- `D` is the delay time in samples
- `x[n - D]` is the delayed input sample

### Interpolation and Fractional Delays
For precise delay times that don't align with sample boundaries, the system employs interpolation techniques:

1. **Linear Interpolation**: Simple but effective for most applications
2. **Cubic Interpolation**: Higher quality for critical applications
3. **Fractional Sample Handling**: Maintains phase accuracy across the audio spectrum

### Feedback and Stability
The feedback system introduces potential instability that must be carefully managed:

```
H(z) = 1 / (1 - g * z^(-D))
```

The system remains stable when `|g| < 1`, but practical considerations often require more conservative limits to prevent runaway oscillation.

## Architectural Design

### Interface Abstraction
The base class defines a pure virtual interface that all delay implementations must satisfy:

```cpp
class DelayLine {
public:
    virtual void processBlock(juce::AudioBuffer<float>& buffer) = 0;
    virtual void prepare(double sampleRate, double maxDelayTime, int numChannels) = 0;
    virtual void reset() = 0;
    // ... other common interface methods
};
```

### Common Infrastructure
Shared functionality includes:
- **Parameter validation and clamping**
- **Sample rate conversion and scaling**
- **Channel management and routing**
- **Memory allocation and management**
- **Smoothing and change detection**

### Factory Pattern Integration
The system uses a factory pattern to instantiate appropriate delay types:

```cpp
enum class DelayType { Digital, BBDelay, Analog };
std::unique_ptr<DelayLine> create(DelayType type);
```

## Parameter Management

### Sonic vs Technical Parameters
The design clearly separates parameters into two categories:

**Sonic Parameters (Config struct):**
- Delay time and feedback
- Wet/dry mixing ratios
- Filter characteristics
- Modulation parameters

**Technical Parameters (Member variables):**
- Sample rate and buffer sizes
- Processing state flags
- Memory management
- Change detection

### Parameter Smoothing
To prevent audio clicks during parameter changes, the system implements automatic smoothing:

```
smooth(t) = start + (target - start) * (1 - e^(-t/τ))
```

Where τ is the smoothing time constant, typically 20-50ms for most parameters.

## Memory Management

### Circular Buffer Implementation
The delay line uses a circular buffer architecture that minimizes memory allocation:

1. **Pre-allocated buffers** sized for maximum delay time
2. **Write and read pointers** that wrap around buffer boundaries
3. **Efficient memory access** with minimal cache misses
4. **Automatic buffer resizing** when requirements change

### Memory Layout Optimization
Memory is organized for optimal performance:
- **Interleaved channel data** for SIMD processing
- **Aligned memory boundaries** for efficient CPU access
- **Cache-friendly buffer sizes** to minimize memory latency

## Performance Characteristics

### Computational Complexity
- **Per-sample processing**: O(1) for basic delay operations
- **Parameter changes**: O(1) with smoothing overhead
- **Buffer management**: O(log n) for dynamic resizing
- **Memory access**: Optimized for sequential access patterns

### Latency Considerations
- **Processing latency**: 0 samples (real-time)
- **Parameter smoothing**: Configurable ramp times
- **Buffer switching**: Seamless during size changes

## Extensibility and Future Design

### Plugin Architecture
The system is designed for easy extension:
- **New delay types** can inherit from base class
- **Parameter additions** follow established patterns
- **Processing algorithms** can be overridden selectively

### Integration Points
Key areas for future enhancement:
- **Advanced interpolation methods**
- **Multi-tap delay networks**
- **Spatial positioning algorithms**
- **Machine learning parameter optimization**

## Design Principles Summary

1. **Separation of Concerns**: Interface, implementation, and configuration are clearly separated
2. **Thread Safety**: All sonic parameters use atomic operations for real-time safety
3. **Performance First**: Memory layout and algorithms optimized for audio processing
4. **Extensibility**: Easy to add new delay types and features
5. **Consistency**: All implementations follow the same architectural patterns
6. **Musicality**: Parameters organized around creative rather than technical needs

This architecture provides a solid foundation for building sophisticated delay effects while maintaining the flexibility to explore new technologies and processing approaches.
