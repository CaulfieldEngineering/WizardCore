# DelayLine Test Plan - Method-Based

## Overview
This test plan organizes tests by individual methods of the DelayLine class, providing clear coverage for each public interface and implementation detail.

## Constructor and Destructor Tests

### DelayLine()
- **Objective**: Verify default constructor behavior
- **Test Cases**:
  - Creates valid object with default state
  - All member variables initialized to expected defaults
  - No memory allocation occurs
  - Object is not prepared initially

### ~DelayLine()
- **Objective**: Verify proper cleanup
- **Test Cases**:
  - Destructor completes without errors
  - All allocated memory is freed
  - No memory leaks detected
  - Multiple destruction cycles work correctly

## Core Setup Methods

### prepare(double sampleRate, double maxDelayTimeInSeconds, int numChannels)
- **Objective**: Verify proper initialization and resource allocation
- **Test Cases**:
  - **Valid parameters**:
    - Sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz
    - Delay times: 0.1s, 1.0s, 2.0s, 5.0s
    - Channel counts: 1, 2, 4, 8, 16
  - **Invalid parameters**:
    - Negative sample rate
    - Zero sample rate
    - Negative delay time
    - Zero delay time
    - Negative channel count
    - Zero channel count
  - **Multiple calls**:
    - Re-preparing with different parameters
    - Re-preparing with same parameters
    - Memory reallocation verification
  - **State verification**:
    - isPrepared() returns true after successful prepare
    - Buffer sizes calculated correctly
    - Write indices initialized to zero

### clear()
- **Objective**: Verify buffer clearing functionality
- **Test Cases**:
  - Clears all channel buffers to zero
  - Resets write indices to zero
  - Maintains prepared state
  - Works with different channel counts
  - Works with different buffer sizes

## Parameter Setting Methods

### setDelayTime(double delayTimeInSeconds)
- **Objective**: Verify delay time setting with smoothing
- **Test Cases**:
  - **Valid delay times**:
    - Within range: 0.0s to max delay time
    - Boundary values: 0.0s, max delay time
    - Fractional delays: 0.001s, 0.5s, 1.5s
  - **Invalid delay times**:
    - Negative values (should clamp to 0.0)
    - Beyond max delay (should clamp to max)
  - **Smoothing behavior**:
    - Smooth transitions between values
    - Respects smoothing time setting
    - No discontinuities during changes

### setDelayInSamples(double delayInSamples)
- **Objective**: Verify sample-accurate delay setting
- **Test Cases**:
  - **Valid sample delays**:
    - Integer samples: 1, 10, 100, 1000
    - Fractional samples: 0.5, 1.5, 10.5
    - Within buffer limits
  - **Invalid sample delays**:
    - Negative values (should clamp)
    - Beyond buffer size (should clamp)
  - **Precision verification**:
    - Fractional sample accuracy
    - Sample rate independence

### setDelayTimeImmediate(double delayTimeInSeconds)
- **Objective**: Verify instant delay time changes
- **Test Cases**:
  - **Valid delay times**: Same as setDelayTime
  - **Bypass smoothing**: Changes applied immediately
  - **Audio artifacts**: Potential clicks during playback
  - **Use cases**: Initialization, reset scenarios

### setSmoothingTime(double rampTimeInSeconds)
- **Objective**: Verify smoothing time configuration
- **Test Cases**:
  - **Valid smoothing times**:
    - 0.0s (no smoothing)
    - 0.01s, 0.05s, 0.1s, 1.0s
  - **Invalid smoothing times**:
    - Negative values (should clamp to 0.0)
  - **Smoothing behavior**:
    - Affects subsequent delay changes
    - Smoothing curve verification

### setInterpolationType(InterpolationType type)
- **Objective**: Verify interpolation mode switching
- **Test Cases**:
  - **Mode switching**:
    - None to Linear
    - Linear to None
    - Multiple switches
  - **Behavior verification**:
    - Interpolation quality differences
    - Performance impact measurement

## Getter Methods

### getDelayTime() const
- **Objective**: Verify delay time retrieval
- **Test Cases**:
  - Returns correct delay time in seconds
  - Handles fractional delays correctly
  - Returns 0.0 when not prepared
  - Consistent with setDelayTime values

### getDelayInSamples() const
- **Objective**: Verify sample delay retrieval
- **Test Cases**:
  - Returns correct delay in samples
  - Handles fractional samples correctly
  - Returns 0.0 when not prepared
  - Consistent with setDelayInSamples values

### getMaxDelayTime() const
- **Objective**: Verify maximum delay time retrieval
- **Test Cases**:
  - Returns value set in prepare()
  - Returns 0.0 when not prepared
  - Consistent across multiple prepare calls

### getMaxDelayInSamples() const
- **Objective**: Verify maximum sample delay retrieval
- **Test Cases**:
  - Returns correct sample count
  - Accounts for sample rate
  - Returns 0 when not prepared

### getSampleRate() const
- **Objective**: Verify sample rate retrieval
- **Test Cases**:
  - Returns value set in prepare()
  - Returns default when not prepared
  - Consistent across multiple prepare calls

### getCurrentDelayInSamples() const
- **Objective**: Verify current smoothed delay retrieval
- **Test Cases**:
  - Returns smoothed delay value
  - Updates during smoothing transitions
  - Consistent with actual processing delay

### getInterpolationType() const
- **Objective**: Verify interpolation type retrieval
- **Test Cases**:
  - Returns currently set interpolation type
  - Default value verification
  - Consistent with setInterpolationType

### isPrepared() const
- **Objective**: Verify prepared state checking
- **Test Cases**:
  - Returns false before prepare()
  - Returns true after successful prepare()
  - Returns false after invalid prepare()
  - Thread-safe state checking

## Audio Processing Methods

### processSample(int channel, float input)
- **Objective**: Verify single sample processing
- **Test Cases**:
  - **Valid processing**:
    - Single channel processing
    - Multi-channel processing
    - Different input values: -1.0, 0.0, 1.0, random
    - Different delay times
  - **Error conditions**:
    - Invalid channel index (negative, beyond count)
    - Processing before prepare()
    - Channel count mismatches
  - **Output verification**:
    - Correct delay timing
    - Proper interpolation
    - No signal degradation

### processBlock(juce::AudioBuffer<float>& buffer)
- **Objective**: Verify block processing without mix
- **Test Cases**:
  - **Valid processing**:
    - Different block sizes: 64, 128, 256, 512
    - Mono and stereo buffers
    - Different delay times
    - Different interpolation types
  - **Error conditions**:
    - Processing before prepare()
    - Null buffer
    - Buffer size mismatches
  - **Output verification**:
    - Complete buffer processing
    - Correct delay timing
    - No buffer corruption

### processBlock(juce::AudioBuffer<float>& buffer, float dryWetMix)
- **Objective**: Verify block processing with mix control
- **Test Cases**:
  - **Mix values**:
    - 0.0 (full dry)
    - 0.5 (50/50 mix)
    - 1.0 (full wet)
    - Extreme values: 0.0, 1.0
  - **Processing verification**:
    - Correct mix ratios
    - Smooth mix transitions
    - No signal clipping
  - **Error conditions**:
    - Invalid mix values (negative, > 1.0)
    - Processing before prepare()

## Private Helper Methods (Internal Testing)

### processSampleLinearInterp(int channel, float input)
- **Objective**: Verify linear interpolation processing
- **Test Cases**:
  - Fractional delay accuracy
  - Interpolation quality
  - Performance measurement
  - Comparison with non-interpolated output

### processSampleNoInterp(int channel, float input)
- **Objective**: Verify nearest-neighbor processing
- **Test Cases**:
  - Integer delay accuracy
  - Nearest sample selection
  - Performance measurement
  - Comparison with interpolated output

### getCurrentDelayInSamples() const (private)
- **Objective**: Verify smoothed delay calculation
- **Test Cases**:
  - Smoothing curve verification
  - Update rate consistency
  - Thread safety verification

## Integration Test Scenarios

### Chorus Effect Simulation
- **Methods involved**: prepare, setDelayTime, processBlock
- **Test Cases**:
  - 20ms base delay with ±5ms modulation
  - Stereo processing
  - Mix control verification

### Flanger Effect Simulation
- **Methods involved**: prepare, setDelayTime, processBlock
- **Test Cases**:
  - 5ms base delay with ±2ms modulation
  - High-frequency modulation
  - Feedback simulation

### Echo Effect Simulation
- **Methods involved**: prepare, setDelayTime, processBlock
- **Test Cases**:
  - Long delays (500ms, 1s)
  - Multiple delay times
  - Mix control verification

## Performance Benchmarks

### CPU Performance Tests
- **Methods**: processSample, processBlock
- **Metrics**:
  - Processing time per sample
  - CPU usage percentage
  - Real-time capability verification

### Memory Performance Tests
- **Methods**: prepare, clear
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
- **Methods**: processSample, processBlock
- **Test Cases**:
  - Audio processing under load
  - Parameter changes during playback
  - No audio dropouts verification 