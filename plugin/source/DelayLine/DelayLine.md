# DelayLine Class

A basic delay unit implementation that functions as a z^-1 block, providing sample-accurate delay functionality.

## Features

- Simple mono delay implementation
- Sample-accurate delay time control
- Circular buffer implementation for efficient memory usage
- Clear functionality to reset the delay line

## Implementation Checklist

### 1. Project Setup
- [ ] Add DelayLine files to project
- [ ] Include DelayLine header in processor
- [ ] Add DelayLine member variable to processor

### 2. Parameters
- [ ] Add delay time parameter (0.0 to 2.0 seconds)
- [ ] Initialize parameters in constructor

### 3. Initialization
- [ ] Initialize delay line in prepareToPlay()
- [ ] Set maximum delay time (recommended: 2.0 seconds)

### 4. Processing
- [ ] Update delay time from parameter in processBlock()
- [ ] Process each channel through delay line
- [ ] Mix delayed signal with input using mix parameter

## Methods

### `prepare(double sampleRate, double maxDelayTimeInSeconds)`
Initializes the delay line with the given sample rate and maximum delay time.

### `setDelayTime(double delayTimeInSeconds)`
Sets the current delay time in seconds. The delay time will be clamped to the maximum delay time set in prepare().

### `double getDelayTime() const`
Returns the current delay time in seconds.

### `float process(float input)`
Processes a single sample through the delay line. Returns the delayed sample.

### `clear()`
Clears the delay line buffer and resets the read/write indices.

## Implementation Details

The delay line uses a circular buffer implementation with a single channel. The buffer size is calculated based on the maximum delay time and sample rate. The read and write indices wrap around using modulo arithmetic to maintain the circular nature of the buffer.

## Usage Notes

- This is a basic delay unit - all additional functionality (mixing, feedback, modulation) should be implemented at the plugin level
- For stereo effects, create two instances of DelayLine
- For modulation effects, use an LFO to modulate the delay time parameter
- For feedback effects, feed the output back into the input at the plugin level

## Future Enhancements

Potential enhancements at the plugin level could include:
- Feedback paths
- Modulation via LFO
- Stereo width control
- Filtering of the delayed signal
- Different delay patterns (ping-pong, multi-tap, etc.)
