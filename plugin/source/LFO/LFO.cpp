#include "LFO.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

LFO::LFO()
{
    // Initialize wavetable with a safe default size
    waveTable.resize(DEFAULT_WAVETABLE_SIZE, 0.0f);
    initializeWaveTable();
}

LFO::~LFO() = default;

void LFO::prepare(double newSampleRate)
{
    // Validate input
    if (newSampleRate <= 0.0 || newSampleRate > 1000000.0) {
        jassertfalse; // Invalid sample rate
        return;
    }
    
    // Update sample rate atomically
    sampleRate.store(newSampleRate);
    
    // Initialize smoothed parameters with reasonable smoothing time (50ms)
    smoothedDepth.reset(newSampleRate, 0.05);
    smoothedDepth.setCurrentAndTargetValue(1.0f);
    
    smoothedSymmetry.reset(newSampleRate, 0.05);
    smoothedSymmetry.setCurrentAndTargetValue(0.5f);
    
    // Initialize wavetable (thread-safe as it's only called during prepare)
    initializeWaveTable();
    
    // Reset position and calculate initial increment
    position.store(0.0f);
    updateIncrement();
    
    // Mark as prepared
    prepared.store(true);
}

void LFO::setFrequency(double frequencyInHz)
{
    // Clamp frequency to valid range and store atomically
    double clampedFreq = std::clamp(frequencyInHz, MIN_FREQUENCY, MAX_FREQUENCY);
    frequency.store(clampedFreq);
    
    // Update increment if prepared
    if (prepared.load()) {
        updateIncrement();
    }
}

void LFO::setDepth(float newDepth)
{
    // Clamp depth to valid range and set target for smoothing
    float clampedDepth = std::clamp(newDepth, 0.0f, 1.0f);
    smoothedDepth.setTargetValue(clampedDepth);
}

void LFO::setPhaseOffset(double phaseOffsetInRadians)
{
    // Store phase offset atomically (no need to wrap - handled in lookup)
    phaseOffset.store(phaseOffsetInRadians);
}

void LFO::setInvert(bool shouldInvert)
{
    // Store invert flag atomically
    invert.store(shouldInvert);
}

void LFO::setSymmetry(float symmetryPercent)
{
    // Clamp symmetry to valid range and convert to 0.0-1.0 range
    float clampedSymmetry = std::clamp(symmetryPercent, 10.0f, 90.0f) / 100.0f;
    smoothedSymmetry.setTargetValue(clampedSymmetry);
}

float LFO::getNextSample()
{
    // Early return if not prepared
    if (!prepared.load()) {
        return 0.0f;
    }
    
    // Get current values
    float currentPos = position.load();
    float currentIncrement = increment.load();
    float currentDepth = smoothedDepth.getNextValue();
    double currentPhaseOffset = phaseOffset.load();
    bool currentInvert = invert.load();
    float currentSymmetry = smoothedSymmetry.getNextValue();
    
    // Apply phase offset to position
    float offsetPos = currentPos + static_cast<float>((currentPhaseOffset / TWO_PI) * waveTable.size());
    
    // Wrap the offset position
    while (offsetPos >= static_cast<float>(waveTable.size())) {
        offsetPos -= static_cast<float>(waveTable.size());
    }
    while (offsetPos < 0.0f) {
        offsetPos += static_cast<float>(waveTable.size());
    }
    
    // Apply symmetry transformation
    float symmetryPos = offsetPos;
    if (currentSymmetry != 0.5f) {
        float normalizedPos = offsetPos / static_cast<float>(waveTable.size()); // [0, 1]
        float remappedPos;
        
        // Symmetry determines the crossover point (where first half ends)
        // 10% = first half compressed to 10% of period, second half gets 90%
        // 90% = first half expanded to 90% of period, second half gets 10%
        
        if (normalizedPos < currentSymmetry) {
            // First half of the waveform: map [0, symmetry] back to [0, 0.5] of sine wave
            remappedPos = (normalizedPos / currentSymmetry) * 0.5f;
        } else {
            // Second half of the waveform: map [symmetry, 1.0] back to [0.5, 1.0] of sine wave
            remappedPos = ((normalizedPos - currentSymmetry) / (1.0f - currentSymmetry)) * 0.5f + 0.5f;
        }
        
        // Convert back to wavetable position
        symmetryPos = remappedPos * static_cast<float>(waveTable.size());
        
        // Ensure we stay within bounds
        if (symmetryPos >= static_cast<float>(waveTable.size())) {
            symmetryPos = static_cast<float>(waveTable.size()) - 1.0f;
        }
    }
    
    // Simple wavetable lookup with linear interpolation
    int index1 = static_cast<int>(symmetryPos);
    int index2 = (index1 + 1) % static_cast<int>(waveTable.size());
    float fraction = symmetryPos - static_cast<float>(index1);
    
    // Bounds safety
    if (index1 >= static_cast<int>(waveTable.size())) index1 = 0;
    if (index2 >= static_cast<int>(waveTable.size())) index2 = 0;
    
    // Linear interpolation
    float sample1 = waveTable[index1];
    float sample2 = waveTable[index2];
    float output = sample1 + fraction * (sample2 - sample1);
    
    // Apply inversion if enabled (flip within [0,1] range)
    if (currentInvert) {
        output = 1.0f - output;
    }
    
    // Apply depth scaling
    output *= currentDepth;
    
    // **THIS IS THE ONLY PLACE WE INCREMENT POSITION**
    float newPos = currentPos + currentIncrement;
    if (newPos >= static_cast<float>(waveTable.size())) {
        newPos -= static_cast<float>(waveTable.size());
    }
    position.store(newPos);
    
    // Temp Debug - Add this after calculating output, before returning:
    static int debugCount = 0;
    if (++debugCount % 100 == 0) {
        DBG("LFO\t" 
            << "pos:" << currentPos 
            << "\tinc:" << currentIncrement 
            << "\toffset:" << offsetPos 
            << "\tidx:" << index1 << "." << (int)(fraction * 100)
            << "\tout:" << output);
    }


    return output;
}

float LFO::getCurrentSample() const
{
    // Early return if not prepared
    if (!prepared.load()) {
        return 0.0f;
    }
    
    // Get current values WITHOUT advancing position
    float currentPos = position.load();
    float currentDepth = smoothedDepth.getCurrentValue();
    double currentPhaseOffset = phaseOffset.load();
    bool currentInvert = invert.load();
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    
    // Apply phase offset to position
    float offsetPos = currentPos + static_cast<float>((currentPhaseOffset / TWO_PI) * waveTable.size());
    
    // Wrap the offset position
    while (offsetPos >= static_cast<float>(waveTable.size())) {
        offsetPos -= static_cast<float>(waveTable.size());
    }
    while (offsetPos < 0.0f) {
        offsetPos += static_cast<float>(waveTable.size());
    }
    
    // Apply symmetry transformation
    float symmetryPos = offsetPos;
    if (currentSymmetry != 0.5f) {
        float normalizedPos = offsetPos / static_cast<float>(waveTable.size()); // [0, 1]
        float remappedPos;
        
        // Symmetry determines the crossover point (where first half ends)
        // 10% = first half compressed to 10% of period, second half gets 90%
        // 90% = first half expanded to 90% of period, second half gets 10%
        
        if (normalizedPos < currentSymmetry) {
            // First half of the waveform: map [0, symmetry] back to [0, 0.5] of sine wave
            remappedPos = (normalizedPos / currentSymmetry) * 0.5f;
        } else {
            // Second half of the waveform: map [symmetry, 1.0] back to [0.5, 1.0] of sine wave
            remappedPos = ((normalizedPos - currentSymmetry) / (1.0f - currentSymmetry)) * 0.5f + 0.5f;
        }
        
        // Convert back to wavetable position
        symmetryPos = remappedPos * static_cast<float>(waveTable.size());
        
        // Ensure we stay within bounds
        if (symmetryPos >= static_cast<float>(waveTable.size())) {
            symmetryPos = static_cast<float>(waveTable.size()) - 1.0f;
        }
    }
    
    // Simple wavetable lookup with linear interpolation
    int index1 = static_cast<int>(symmetryPos);
    int index2 = (index1 + 1) % static_cast<int>(waveTable.size());
    float fraction = symmetryPos - static_cast<float>(index1);
    
    // Bounds safety
    if (index1 >= static_cast<int>(waveTable.size())) index1 = 0;
    if (index2 >= static_cast<int>(waveTable.size())) index2 = 0;
    
    // Linear interpolation
    float sample1 = waveTable[index1];
    float sample2 = waveTable[index2];
    float output = sample1 + fraction * (sample2 - sample1);
    
    // Apply inversion if enabled (flip within [0,1] range)
    if (currentInvert) {
        output = 1.0f - output;
    }
    
    // Apply depth scaling
    return output * currentDepth;
}

void LFO::reset()
{
    position.store(0.0f);
}

void LFO::reset(double phaseInRadians)
{
    if (waveTable.empty()) {
        position.store(0.0f);
        return;
    }
    
    // Convert phase to wavetable position safely
    double normalizedPhase = std::fmod(phaseInRadians, TWO_PI);
    if (normalizedPhase < 0.0) {
        normalizedPhase += TWO_PI;
    }
    
    float newPos = static_cast<float>((normalizedPhase / TWO_PI) * waveTable.size());
    newPos = std::clamp(newPos, 0.0f, static_cast<float>(waveTable.size()) - 1.0f);
    position.store(newPos);
}

float LFO::getPosition() const
{
    return position.load();
}

void LFO::setPosition(float newPosition)
{
    if (waveTable.empty()) {
        position.store(0.0f);
        return;
    }
    
    // Wrap position to valid range
    float wrappedPos = std::fmod(newPosition, static_cast<float>(waveTable.size()));
    if (wrappedPos < 0.0f) {
        wrappedPos += static_cast<float>(waveTable.size());
    }
    position.store(wrappedPos);
}

int LFO::getWaveTableSize() const
{
    return static_cast<int>(waveTable.size());
}

// Private helper methods

void LFO::initializeWaveTable()
{
    // Ensure wavetable has a reasonable size
    if (waveTable.size() != DEFAULT_WAVETABLE_SIZE) {
        waveTable.resize(DEFAULT_WAVETABLE_SIZE);
    }
    
    // Generate sine wave with proper [0,1] mapping
    const int tableSize = static_cast<int>(waveTable.size());
    for (int i = 0; i < tableSize; ++i) {
        // Generate sine wave from 0 to 2π
        double phase = (TWO_PI * i) / tableSize;
        float sineValue = static_cast<float>(std::sin(phase));
        
        // Convert [-1, +1] to [0, 1] range
        waveTable[i] = (sineValue * 0.5f) + 0.5f;
    }
}

void LFO::updateIncrement()
{
    // Calculate increment safely
    double currentSampleRate = sampleRate.load();
    double currentFrequency = frequency.load();
    
    if (currentSampleRate <= 0.0 || waveTable.empty()) {
        increment.store(0.0f);
        return;
    }
    
    // Calculate increment: (frequency * tableSize) / sampleRate
    double newIncrement = (currentFrequency * waveTable.size()) / currentSampleRate;
    
    // Clamp to reasonable bounds to prevent overflow
    newIncrement = std::clamp(newIncrement, 0.0, static_cast<double>(waveTable.size()) * 0.5);
    
    increment.store(static_cast<float>(newIncrement));
}

} // namespace audio_plugin 