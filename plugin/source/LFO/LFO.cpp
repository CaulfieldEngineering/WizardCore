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

void LFO::setSyncToHost(bool shouldSync)
{
    syncToHost.store(shouldSync);
    // Recalculate increment when sync mode changes
    if (prepared.load()) {
        updateIncrement();
    }
}

void LFO::setSyncRate(int syncRateIndex)
{
    // Clamp to valid range
    int clampedIndex = std::clamp(syncRateIndex, 0, 5);
    int oldIndex = this->syncRateIndex.load();
    
    // Debug output - always show what we're trying to set
    const char* syncNames[] = {"1/2 Note", "1/4 Note", "1/4 Triplet", "1/8 Note", "1/8 Triplet", "1/16 Note"};
    DBG("LFO setSyncRate called: requested=" << syncRateIndex << ", clamped=" << clampedIndex 
        << " (" << syncNames[clampedIndex] << "), current=" << oldIndex);
    
    this->syncRateIndex.store(clampedIndex);
    
    // Debug output when sync rate changes
    if (oldIndex != clampedIndex) {
        DBG("LFO Sync Rate changed from " << oldIndex << " (" << syncNames[oldIndex] << ") to " 
            << clampedIndex << " (" << syncNames[clampedIndex] << ")");
    } else {
        DBG("LFO Sync Rate unchanged at " << clampedIndex << " (" << syncNames[clampedIndex] << ")");
    }
    
    // Recalculate increment if in sync mode
    if (prepared.load() && syncToHost.load()) {
        DBG("LFO setSyncRate: Calling updateIncrement (sync mode active)");
        updateIncrement();
    } else {
        DBG("LFO setSyncRate: Not calling updateIncrement (prepared=" << prepared.load() << ", syncToHost=" << syncToHost.load() << ")");
    }
}

void LFO::updateHostInfo(double bpm, bool isPlaying)
{
    hostBPM.store(bpm);
    hostIsPlaying.store(isPlaying);
    
    // Recalculate increment if in sync mode
    if (prepared.load() && syncToHost.load()) {
        updateIncrement();
    }
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

float LFO::calculateSyncFrequency() const
{
    double currentBPM = hostBPM.load();
    int currentSyncRate = syncRateIndex.load();
    
    if (currentBPM <= 0.0) {
        return 1.0f;  // Fallback frequency
    }
    
    // Convert BPM to beats per second
    float beatsPerSecond = static_cast<float>(currentBPM / 60.0);
    
    // Musical division multipliers (frequency = BPM/60 * multiplier)
    const float syncRateMultipliers[] = {
        0.5f,     // 1/2 Note    = 0.5x BPM (slower - 2 beats per cycle)
        1.0f,     // 1/4 Note    = 1.0x BPM (normal - 1 beat per cycle)
        1.5f,     // 1/4 Triplet = 1.5x BPM (3 triplets per 2 beats)
        2.0f,     // 1/8 Note    = 2.0x BPM (faster - 0.5 beats per cycle)
        3.0f,     // 1/8 Triplet = 3.0x BPM (3 triplets per beat)
        4.0f      // 1/16 Note   = 4.0x BPM (fastest - 0.25 beats per cycle)
    };
    
    float resultFreq = 1.0f;  // Fallback
    
    if (currentSyncRate >= 0 && currentSyncRate < 6) {
        const char* syncNames[] = {"1/2 Note", "1/4 Note", "1/4 Triplet", "1/8 Note", "1/8 Triplet", "1/16 Note"};
        resultFreq = beatsPerSecond * syncRateMultipliers[currentSyncRate];
        
        // Debug output to track sync frequency calculations
        DBG("LFO Sync: BPM=" << currentBPM << ", SyncRateIndex=" << currentSyncRate 
            << " (" << syncNames[currentSyncRate] << "), Multiplier=" << syncRateMultipliers[currentSyncRate] 
            << ", BeatsPerSec=" << beatsPerSecond << ", ResultFreq=" << resultFreq << "Hz");
    } else {
        DBG("LFO Sync: Invalid sync rate index " << currentSyncRate << ", using fallback");
    }
    
    return resultFreq;
}

void LFO::updateIncrement()
{
    // Calculate increment safely
    double currentSampleRate = sampleRate.load();
    
    if (currentSampleRate <= 0.0 || waveTable.empty()) {
        increment.store(0.0f);
        return;
    }
    
    float currentFrequency;
    bool usingSyncMode = syncToHost.load();
    
    if (usingSyncMode) {
        // Use host sync frequency and apply the same clamping as manual frequency
        float rawSyncFreq = calculateSyncFrequency();
        currentFrequency = std::clamp(rawSyncFreq, static_cast<float>(MIN_FREQUENCY), static_cast<float>(MAX_FREQUENCY));
        
        DBG("LFO updateIncrement: Sync mode - Raw=" << rawSyncFreq << "Hz, Clamped=" << currentFrequency << "Hz");
    } else {
        // Use manual frequency
        currentFrequency = static_cast<float>(frequency.load());
        
        DBG("LFO updateIncrement: Manual mode - Frequency=" << currentFrequency << "Hz");
    }
    
    // Calculate increment: (frequency * tableSize) / sampleRate
    double newIncrement = (currentFrequency * waveTable.size()) / currentSampleRate;
    
    // Clamp to reasonable bounds to prevent overflow
    double clampedIncrement = std::clamp(newIncrement, 0.0, static_cast<double>(waveTable.size()) * 0.5);
    
    DBG("LFO increment: Freq=" << currentFrequency << "Hz, TableSize=" << waveTable.size() 
        << ", SampleRate=" << currentSampleRate << ", RawIncrement=" << newIncrement 
        << ", ClampedIncrement=" << clampedIncrement);
    
    increment.store(static_cast<float>(clampedIncrement));
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

} // namespace audio_plugin 