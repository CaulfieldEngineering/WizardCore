# LFO Test Plan - Method-Based

## Overview
This test plan organizes tests by individual methods of the LFO class, providing clear coverage for each public interface and implementation detail.

## Constructor and Destructor Tests

### LFO()
- **Objective**: Verify default constructor behavior
- **Test Cases**:
  - Creates valid object with default state
  - All member variables initialized to expected defaults
  - No memory allocation occurs
  - Object is not prepared initially

### ~LFO()
- **Objective**: Verify proper cleanup
- **Test Cases**:
  - Destructor completes without errors
  - No memory leaks detected
  - Multiple destruction cycles work correctly

## Core Setup Methods

### prepare(double sampleRate)
- **Objective**: Verify proper initialization and state setup
- **Test Cases**:
  - **Valid sample rates**:
    - 44.1kHz, 48kHz, 96kHz, 192kHz
    - High sample rates: 384kHz
  - **Invalid sample rates**:
    - Negative values
    - Zero values
    - Very large values
  - **State verification**:
    - isPrepared() returns true after successful prepare
    - Phase accumulator initialized to 0.0
    - Phase increment calculated correctly
  - **Multiple calls**:
    - Re-preparing with different sample rates
    - Re-preparing with same sample rate
    - State consistency verification

## Parameter Setting Methods

### setFrequency(double frequencyInHz)
- **Objective**: Verify frequency parameter handling
- **Test Cases**:
  - **Valid frequencies**:
    - Low frequencies: 0.1Hz, 0.5Hz, 1Hz
    - Mid frequencies: 2Hz, 5Hz, 10Hz
    - High frequencies: 15Hz, 20Hz
    - Boundary values: 0.1Hz, 20Hz
  - **Invalid frequencies**:
    - Negative values (should clamp to 0.1Hz)
    - Zero values (should clamp to 0.1Hz)
    - Beyond maximum (should clamp to 20Hz)
  - **Phase increment verification**:
    - Correct calculation: (2π × frequency) / sampleRate
    - Updates immediately without smoothing
    - Maintains phase accuracy

### setDepth(float depth)
- **Objective**: Verify depth parameter handling
- **Test Cases**:
  - **Valid depth values**:
    - 0.0 (no modulation)
    - 0.1, 0.5, 1.0 (full modulation)
    - Boundary values: 0.0, 1.0
  - **Invalid depth values**:
    - Negative values (should clamp to 0.0)
    - Beyond 1.0 (should clamp to 1.0)
  - **Output range verification**:
    - Affects output range in Bipolar and Unipolar modes
    - Ignored in Normalized mode

### setNormalizationMode(NormalizationMode mode)
- **Objective**: Verify normalization mode switching
- **Test Cases**:
  - **Mode switching**:
    - Bipolar to Unipolar
    - Unipolar to Normalized
    - Normalized to Bipolar
    - Multiple switches
  - **Output range verification**:
    - Bipolar: [-depth, +depth]
    - Unipolar: [0, +depth]
    - Normalized: [0, 1]
  - **Mode persistence**:
    - Mode maintained across frequency changes
    - Mode maintained across depth changes

### setPhaseOffset(double phaseOffsetInRadians)
- **Objective**: Verify phase offset functionality
- **Test Cases**:
  - **Valid phase offsets**:
    - 0.0, π/2, π, 3π/2, 2π
    - Fractional values: 0.1, 1.5, 5.5
  - **Phase wrapping**:
    - Values beyond 2π (should wrap)
    - Negative values (should wrap)
    - Large values: 10π, -5π
  - **Output verification**:
    - Phase relationship maintained
    - Offset applied correctly to output

### setWaveformType(WaveformType type)
- **Objective**: Verify waveform type switching
- **Test Cases**:
  - **Valid waveform types**:
    - Sine wave (currently only supported)
    - Future waveform types (Triangle, Square, etc.)
  - **Fallback behavior**:
    - Unsupported types fall back to Sine
    - No errors thrown for unsupported types
  - **Output verification**:
    - Waveform shape changes correctly
    - Performance impact measurement

## Audio Processing Methods

### getNextSample()
- **Objective**: Verify sample generation with phase advancement
- **Test Cases**:
  - **Valid processing**:
    - Different frequencies: 0.1Hz, 1Hz, 10Hz, 20Hz
    - Different depths: 0.1, 0.5, 1.0
    - Different normalization modes
    - Different phase offsets
  - **Error conditions**:
    - Processing before prepare() (should return 0.0)
    - Invalid state handling
  - **Output verification**:
    - Correct output range for each mode
    - Phase advancement verification
    - Frequency accuracy measurement
    - No discontinuities in output

### getCurrentSample() const
- **Objective**: Verify sample generation without phase advancement
- **Test Cases**:
  - **Valid processing**: Same as getNextSample()
  - **Phase verification**:
    - Phase accumulator unchanged
    - Same output as getNextSample() when called consecutively
    - Multiple calls return same value
  - **Error conditions**: Same as getNextSample()

## Reset Methods

### reset()
- **Objective**: Verify phase reset to zero
- **Test Cases**:
  - **Reset behavior**:
    - Phase accumulator set to 0.0
    - Output starts from beginning of cycle
    - No discontinuities in output
  - **Error conditions**:
    - Reset before prepare() (should handle gracefully)
  - **Multiple resets**:
    - Multiple reset calls work correctly
    - Reset during operation

### reset(double phaseInRadians)
- **Objective**: Verify phase reset to specific value
- **Test Cases**:
  - **Valid phase values**:
    - 0.0, π/2, π, 3π/2, 2π
    - Fractional values: 0.1, 1.5, 5.5
  - **Phase wrapping**:
    - Values beyond 2π (should wrap)
    - Negative values (should wrap)
  - **Output verification**:
    - Output starts from specified phase
    - No discontinuities in output

## Getter Methods

### getFrequency() const
- **Objective**: Verify frequency retrieval
- **Test Cases**:
  - Returns currently set frequency
  - Returns default value when not set
  - Consistent with setFrequency values
  - Handles clamped values correctly

### getDepth() const
- **Objective**: Verify depth retrieval
- **Test Cases**:
  - Returns currently set depth
  - Returns default value when not set
  - Consistent with setDepth values
  - Handles clamped values correctly

### getNormalizationMode() const
- **Objective**: Verify normalization mode retrieval
- **Test Cases**:
  - Returns currently set mode
  - Returns default value (Bipolar)
  - Consistent with setNormalizationMode values

### getPhaseOffset() const
- **Objective**: Verify phase offset retrieval
- **Test Cases**:
  - Returns currently set phase offset
  - Returns wrapped values correctly
  - Consistent with setPhaseOffset values

### getWaveformType() const
- **Objective**: Verify waveform type retrieval
- **Test Cases**:
  - Returns currently set waveform type
  - Returns default value (Sine)
  - Consistent with setWaveformType values

### isPrepared() const
- **Objective**: Verify prepared state checking
- **Test Cases**:
  - Returns false before prepare()
  - Returns true after successful prepare()
  - Returns false after invalid prepare()
  - Thread-safe state checking

### getSampleRate() const
- **Objective**: Verify sample rate retrieval
- **Test Cases**:
  - Returns value set in prepare()
  - Returns default when not prepared
  - Consistent across multiple prepare calls

### getCurrentPhase() const
- **Objective**: Verify current phase retrieval
- **Test Cases**:
  - Returns current phase with offset applied
  - Returns wrapped phase values
  - Updates during phase advancement
  - Consistent with internal state

### getOutputRangeDescription() const
- **Objective**: Verify output range description
- **Test Cases**:
  - **Bipolar mode**: "Bipolar: [-depth, +depth]"
  - **Unipolar mode**: "Unipolar: [0, +depth]"
  - **Normalized mode**: "Normalized: [0, 1]"
  - **Format verification**:
    - Correct string format
    - Depth values displayed correctly
    - No formatting errors

## Private Helper Methods (Internal Testing)

### generateSineWave(double phase) const
- **Objective**: Verify sine wave generation
- **Test Cases**:
  - **Phase values**: 0, π/2, π, 3π/2, 2π
  - **Output verification**:
    - Correct sine wave values
    - Precision verification
    - Performance measurement

### wrapPhase(double phase) const
- **Objective**: Verify phase wrapping functionality
- **Test Cases**:
  - **Wrapping behavior**:
    - Values beyond 2π (should wrap)
    - Negative values (should wrap)
    - Large values: 10π, -5π
    - Small values: 0.1, -0.1
  - **Precision verification**:
    - Accurate wrapping calculations
    - No accumulation errors

### applyNormalization(float rawValue) const
- **Objective**: Verify normalization application
- **Test Cases**:
  - **Bipolar mode**:
    - Input [-1, +1] → Output [-depth, +depth]
    - Scaling verification
  - **Unipolar mode**:
    - Input [-1, +1] → Output [0, +depth]
    - Offset and scaling verification
  - **Normalized mode**:
    - Input [-1, +1] → Output [0, 1]
    - Depth ignored verification

### updatePhaseIncrement()
- **Objective**: Verify phase increment calculation
- **Test Cases**:
  - **Calculation accuracy**:
    - Correct formula: (2π × frequency) / sampleRate
    - Different frequency values
    - Different sample rates
  - **Update timing**:
    - Called when frequency changes
    - Called when sample rate changes

## Integration Test Scenarios

### Delay Time Modulation
- **Methods involved**: prepare, setFrequency, setDepth, setNormalizationMode, getNextSample
- **Test Cases**:
  - Chorus effect: 1-2Hz, ±5-20ms modulation
  - Flanger effect: 0.1-1Hz, ±1-5ms modulation
  - Vibrato effect: 5-10Hz, ±10-50ms modulation

### Filter Modulation
- **Methods involved**: prepare, setFrequency, setDepth, setNormalizationMode, getNextSample
- **Test Cases**:
  - Cutoff frequency modulation
  - Resonance modulation
  - Modulation depth control

### Amplitude Modulation
- **Methods involved**: prepare, setFrequency, setDepth, setNormalizationMode, getNextSample
- **Test Cases**:
  - Tremolo effect simulation
  - Volume modulation
  - Pan modulation

## Performance Benchmarks

### CPU Performance Tests
- **Methods**: getNextSample, getCurrentSample
- **Metrics**:
  - Processing time per sample
  - CPU usage percentage
  - Real-time capability verification

### Memory Performance Tests
- **Methods**: prepare, constructor, destructor
- **Metrics**:
  - Memory allocation size
  - Memory usage patterns
  - Memory leak detection

## Thread Safety Verification

### Concurrent Access Tests
- **Methods**: All setter and getter methods
- **Test Cases**:
  - Parameter setting during processing
  - Multiple thread access
  - Race condition detection

### Real-time Safety Tests
- **Methods**: getNextSample, getCurrentSample
- **Test Cases**:
  - Audio processing under load
  - Parameter changes during playback
  - No audio dropouts verification 