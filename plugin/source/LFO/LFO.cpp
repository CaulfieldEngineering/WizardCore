#include "LFO.h"
#include <algorithm>
#include <cmath>

// Ensure M_PI is defined if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    // Don't reset depth value - preserve whatever was set via setDepth()
    
    smoothedSymmetry.reset(newSampleRate, 0.05);
    //smoothedSymmetry.setCurrentAndTargetValue(0.5f);
    
    // Initialize wavetable (thread-safe as it's only called during prepare)
    initializeWaveTable();
    
    // Reset position and calculate initial increment
    position.store(0.0f);
    updateIncrement();
    
    // Mark as prepared
    prepared.store(true);
    
    // Reset downbeat tracking
    downbeatDetected.store(false);
    lastDownbeatTime.store(0.0);
    
    // Reset parameter change detection
    firstRun.store(true);
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
    //DBG("LFO Smoothed Depth = " << smoothedDepth.getCurrentValue());
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
    
    // Regenerate wavetable if prepared, since inversion affects the waveshape
    if (prepared.load()) {
        initializeWaveTable();
        // DBG("LFO Invert changed to " << (shouldInvert ? "true" : "false") << ", wavetable regenerated");
    }
}

void LFO::setEnabled(bool shouldEnable)
{
    // Store enabled flag atomically
    enabled.store(shouldEnable);
}

void LFO::setSymmetry(float symmetryPercent)
{
    // Clamp symmetry to valid range and convert to 0.0-1.0 range
    float clampedSymmetry = std::clamp(symmetryPercent, 10.0f, 90.0f) / 100.0f;
    smoothedSymmetry.setTargetValue(clampedSymmetry);
}

void LFO::setWaveShape(WaveformType waveshape)
{
    waveShape.store(waveshape);
    
    // Regenerate wavetable if prepared
    if (prepared.load()) {
        initializeWaveTable();
        // DBG("LFO WaveShape changed to " << static_cast<int>(waveshape) << ", wavetable regenerated");
    }
}

void LFO::setSyncToHost(bool shouldSync)
{
    syncToHost.store(shouldSync);
    
    // Reset downbeat tracking when sync mode changes
    if (shouldSync) {
        downbeatDetected.store(false);
        lastDownbeatTime.store(0.0);
        // DBG("LFO Sync mode enabled - downbeat tracking reset");
    }
    
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
    // DBG("LFO setSyncRate called: requested=" << syncRateIndex << ", clamped=" << clampedIndex 
    //     << " (" << syncNames[clampedIndex] << "), current=" << oldIndex);
    
    this->syncRateIndex.store(clampedIndex);
    
    // Debug output when sync rate changes
    if (oldIndex != clampedIndex) {
        // DBG("LFO Sync Rate changed from " << oldIndex << " (" << syncNames[oldIndex] << ") to " 
        //     << clampedIndex << " (" << syncNames[clampedIndex] << ")");
    } else {
        // DBG("LFO Sync Rate unchanged at " << clampedIndex << " (" << syncNames[clampedIndex] << ")");
    }
    
    // Recalculate increment if in sync mode
    if (prepared.load() && syncToHost.load()) {
        // DBG("LFO setSyncRate: Calling updateIncrement (sync mode active)");
        updateIncrement();
    } else {
        // DBG("LFO setSyncRate: Not calling updateIncrement (prepared=" << (prepared.load() ? "true" : "false") << ", syncToHost=" << (syncToHost.load() ? "true" : "false") << ")");
    }
}

void LFO::setCoupling(CouplingType couplingType)
{
    // Store coupling type atomically
    // Note: Should only be called during initialization, not during audio processing
    coupling.store(couplingType);
}

LFO::CouplingType LFO::getCoupling() const
{
    return coupling.load();
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

void LFO::updateHostInfo(double bpm, bool isPlaying, double beatPosition, double ppqPosition)
{
    // Store host information
    hostBPM.store(bpm);
    hostIsPlaying.store(isPlaying);
    hostBeatPosition.store(beatPosition);
    hostPPQPosition.store(ppqPosition);
    
    // Check for downbeat locking if in sync mode
    if (prepared.load() && syncToHost.load() && isPlaying) {
        // Calculate current time based on BPM and PPQ position
        double beatsPerSecond = bpm / 60.0;
        double currentTime = ppqPosition / beatsPerSecond;
        
        checkForDownbeatLock(currentTime);
        updateIncrement();
    }
}

float LFO::getNextSample()
{
    // Early return if not prepared
    if (!prepared.load()) {
        return 0.0f;
    }
    
    // Early return if disabled - return 1.0 (no modulation)
    if (!enabled.load()) {
        return 1.0f;
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
    //if (currentInvert) {
    //    output = 1.0f - output;
    //}
    
    // Apply coupling transformation (coupling set once at initialization)
    if (coupling.load() == CouplingType::AC) {
        // AC coupling: convert [0,1] to [-1,1]
        output = (output * 2.0f) - 1.0f;
    }
    // DC coupling: keep [0,1] range (no change needed)
    
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
    
    // Early return if disabled - return 1.0 (no modulation)
    if (!enabled.load()) {
        return 1.0f;
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
    //if (currentInvert) {
    //    output = 1.0f - output;
    //}
    
    // Apply coupling transformation (coupling set once at initialization)
    if (coupling.load() == CouplingType::AC) {
        // AC coupling: convert [0,1] to [-1,1]
        output = (output * 2.0f) - 1.0f;
    }
    // DC coupling: keep [0,1] range (no change needed)
    
    // Apply depth scaling
    return output * currentDepth;
}

float LFO::calculateSyncFrequency() const
{
    // Get current values atomically
    double currentBPM = hostBPM.load();
    int currentSyncRate = syncRateIndex.load();
    
    if (currentBPM <= 0.0) {
        return 1.0f;  // Fallback
    }
    
    // Calculate beats per second
    double beatsPerSecond = currentBPM / 60.0;
    
    // Sync rate multipliers for different note divisions
    const double syncRateMultipliers[] = {0.5, 1.0, 1.333, 2.0, 2.667, 4.0};
    
    float resultFreq = 1.0f;  // Fallback
    
    if (currentSyncRate >= 0 && currentSyncRate < 6) {
        const char* syncNames[] = {"1/2 Note", "1/4 Note", "1/4 Triplet", "1/8 Note", "1/8 Triplet", "1/16 Note"};
        resultFreq = beatsPerSecond * syncRateMultipliers[currentSyncRate];
        
        // Debug output to track sync frequency calculations
        // DBG("LFO Sync: BPM=" << currentBPM << ", SyncRateIndex=" << currentSyncRate 
        //     << " (" << syncNames[currentSyncRate] << "), Multiplier=" << syncRateMultipliers[currentSyncRate] 
        //     << ", BeatsPerSec=" << beatsPerSecond << ", ResultFreq=" << resultFreq << "Hz");
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
        
        // DBG("LFO updateIncrement: Sync mode - Raw=" << rawSyncFreq << "Hz, Clamped=" << currentFrequency << "Hz");
    } else {
        // Use manual frequency
        currentFrequency = static_cast<float>(frequency.load());
        
        // DBG("LFO updateIncrement: Manual mode - Frequency=" << currentFrequency << "Hz");
    }
    
    // Calculate increment: (frequency * tableSize) / sampleRate
    double newIncrement = (currentFrequency * waveTable.size()) / currentSampleRate;
    
    // Clamp to reasonable bounds to prevent overflow
    double clampedIncrement = std::clamp(newIncrement, 0.0, static_cast<double>(waveTable.size()) * 0.5);
    
    // DBG("LFO increment: Freq=" << currentFrequency << "Hz, TableSize=" << waveTable.size() 
    //     << ", SampleRate=" << currentSampleRate << ", RawIncrement=" << newIncrement 
    //     << ", ClampedIncrement=" << clampedIncrement);
    
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

juce::String LFO::getWaveShapeName() const
{
    WaveformType currentShape = waveShape.load();
    
    switch (currentShape) {
        case WaveformType::Sine:
            return "Sine";
        case WaveformType::RampDown:
            return "Ramp Down";
        case WaveformType::RampUp:
            return "Ramp Up";
        case WaveformType::Square:
            return "Square";
        case WaveformType::Triangle:
            return "Triangle";
        case WaveformType::HumpDown:
            return "Hump Down";
        case WaveformType::HumpUp:
            return "Hump Up";
        default:
            return "Unknown";
    }
}

// Private helper methods

void LFO::initializeWaveTable()
{
    // Ensure wavetable has a reasonable size
    if (waveTable.size() != DEFAULT_WAVETABLE_SIZE) {
        waveTable.resize(DEFAULT_WAVETABLE_SIZE);
    }
    
    // Generate waveform based on selected waveshape
    WaveformType currentShape = waveShape.load();
    
    switch (currentShape) {
        case WaveformType::Sine:
            generateSineWave();
            break;
        case WaveformType::RampDown:
            generateRampDownWave();
            break;
        case WaveformType::RampUp:
            generateRampUpWave();
            break;
        case WaveformType::Square:
            generateSquareWave();
            break;
        case WaveformType::Triangle:
            generateTriangleWave();
            break;
        case WaveformType::HumpDown:
            generateHumpDownWave();
            break;
        case WaveformType::HumpUp:
            generateHumpUpWave();
            break;
        default:
            generateSineWave(); // Fallback to sine
            break;
    }
}

void LFO::checkForDownbeatLock(double currentTime)
{
    // Check if we're at or very close to a downbeat (beatPosition near 0.0)
    double currentBeatPos = hostBeatPosition.load();
    
    // Consider it a downbeat if we're within 0.1 beats of the start
    // This provides some tolerance for timing variations
    if (currentBeatPos < 0.1 || currentBeatPos > 0.9) {
        // Check if this is a new downbeat (not the same one we already processed)
        double lastDownbeat = lastDownbeatTime.load();
        
        // If this is a new downbeat, reset the LFO phase
        if (std::abs(currentTime - lastDownbeat) > 0.1) { // At least 0.1 seconds difference
            // DBG("LFO Downbeat detected at beat position " << currentBeatPos << ", resetting phase");
            
            // Reset LFO position to start of waveform (phase 0)
            position.store(0.0f);
            
            // Update last downbeat time
            lastDownbeatTime.store(currentTime);
            downbeatDetected.store(true);
        }
    }
}

void LFO::generateSineWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    // Populate the Left-Hand Period
    for (int i = 0; i < periodLeft; ++i) {
        // Generate sine wave from 0 to π (first half)
        double phase = (M_PI * i) / periodLeft;
        float sineValue = static_cast<float>(std::sin(phase));
        
        // Convert [-1, +1] to [0, 1] range
        float y = (sineValue * 0.5f) + 0.5f;
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
    
    // Populate the Right-Hand Period
    for (int i = periodLeft; i < tableSize; ++i) {
        // Generate sine wave from π to 2π (second half)
        double phase = M_PI + (M_PI * (i - periodLeft)) / periodRight;
        float sineValue = static_cast<float>(std::sin(phase));
        
        // Convert [-1, +1] to [0, 1] range
        float y = (sineValue * 0.5f) + 0.5f;
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
}

void LFO::generateRampDownWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    float deltaDown = 0.01f;
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    for (int i = 0; i < tableSize; ++i) {
        // Corner-Rounding Function
        float y = ((1.0f / std::atan(1.0f / deltaDown)) * std::atan(std::sin(M_PI * i / tableSize) / deltaDown));
        
        // Waveshape Function
        float x;
        
        // Populate the Left-Hand Period
        if (i < periodLeft) {
            x = 1.0f + (0.5f) * (-static_cast<float>(i) / periodLeft);
        }
        // Populate the Right-Hand Period
        else {
            x = (0.5f) + (0.5f) * (-static_cast<float>(i - periodLeft) / periodRight);
        }
        
        // Apply inversion if enabled
        if (invert.load()) {
            x = 1.0f - x;
        }
        
        waveTable[i] = y * x;
    }
}

void LFO::generateRampUpWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    float deltaUp = 0.01f;
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    for (int i = 0; i < tableSize; ++i) {
        // Corner-Rounding Function
        float y = ((1.0f / std::atan(1.0f / deltaUp)) * std::atan(std::sin(M_PI * i / tableSize) / deltaUp));
        
        // Waveshape Function
        float x;
        
        // Populate the Left-Hand Period
        if (i < periodLeft) {
            x = (0.5f) * (static_cast<float>(i) / periodLeft);
        }
        // Populate the Right-Hand Period
        else {
            x = (0.5f) + (0.5f) * (static_cast<float>(i - periodLeft) / periodRight);
        }
        
        // Apply inversion if enabled
        if (invert.load()) {
            x = 1.0f - x;
        }
        
        waveTable[i] = x * y;
    }
}

void LFO::generateSquareWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    float deltaPulse = 0.01f;
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    // Populate the Left-Hand Period (high)
    for (int i = 0; i < periodLeft; ++i) {
        float y = 0.5f + ((0.5f / std::atan(1.0f / deltaPulse)) * std::atan(std::sin(M_PI * i / periodLeft) / deltaPulse));
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
    
    // Populate the Right-Hand Period (low)
    for (int i = periodLeft; i < tableSize; ++i) {
        float y = 0.5f - ((0.5f / std::atan(1.0f / deltaPulse)) * std::atan(std::sin(M_PI * (i - periodLeft) / periodRight) / deltaPulse));
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
}

void LFO::generateTriangleWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    // Populate the Left-Hand Period
    for (int i = 0; i < periodLeft; ++i) {
        float y = static_cast<float>(i) / periodLeft;
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
    
    // Populate the Right-Hand Period
    for (int i = periodLeft; i < tableSize; ++i) {
        float y = 1.0f - (static_cast<float>(i - periodLeft) / periodRight);
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
}

void LFO::generateHumpDownWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    // Populate the Left-Hand Period
    for (int i = 0; i < periodLeft; ++i) {
        float y = std::sin(0.5f * M_PI * i / periodLeft);
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
    
    // Populate the Right-Hand Period
    for (int i = periodLeft; i < tableSize; ++i) {
        float y = std::cos(0.5f * M_PI * (i - periodLeft) / periodRight);
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
}

void LFO::generateHumpUpWave()
{
    const int tableSize = static_cast<int>(waveTable.size());
    
    // Calculate periods based on current symmetry setting
    float currentSymmetry = smoothedSymmetry.getCurrentValue();
    int periodLeft = static_cast<int>(tableSize * currentSymmetry);
    int periodRight = tableSize - periodLeft;
    
    // Ensure minimum period sizes
    if (periodLeft < 1) periodLeft = 1;
    if (periodRight < 1) periodRight = 1;
    
    // Populate the Left-Hand Period
    for (int i = 0; i < periodLeft; ++i) {
        float y = (-std::sin(0.5f * M_PI * i / periodLeft) + 1.0f);
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
    
    // Populate the Right-Hand Period
    for (int i = periodLeft; i < tableSize; ++i) {
        float y = (-std::cos(0.5f * M_PI * (i - periodLeft) / periodRight) + 1.0f);
        
        // Apply inversion if enabled
        if (invert.load()) {
            y = 1.0f - y;
        }
        
        waveTable[i] = y;
    }
}

void LFO::updateParameters(float frequency, float depth, bool enabled,
                          bool invert, float phaseOffset, float symmetry, bool syncToHost,
                          int syncRate, WaveformType waveshape)
{
    // Store enabled state
    this->enabled.store(enabled);
    
    // Check frequency changes
    if (frequency != lastFrequency.load() || firstRun.load()) {
        setFrequency(frequency);
        lastFrequency.store(frequency);
    }
    
    // Check depth changes
    if (depth != lastDepth.load() || firstRun.load()) {
        setDepth(depth);
        lastDepth.store(depth);
    }
    
    // Check invert changes
    if (invert != lastInvert.load() || firstRun.load()) {
        setInvert(invert);
        lastInvert.store(invert);
    }
    
    // Check phase offset changes
    if (phaseOffset != lastPhaseOffset.load() || firstRun.load()) {
        setPhaseOffset(phaseOffset * (M_PI / 180.0f)); // Convert degrees to radians
        lastPhaseOffset.store(phaseOffset);
    }
    
    // Check symmetry changes
    if (symmetry != lastSymmetry.load() || firstRun.load()) {
        setSymmetry(symmetry);
        lastSymmetry.store(symmetry);
    }
    
    // Check sync to host changes
    if (syncToHost != lastSyncToHost.load() || firstRun.load()) {
        setSyncToHost(syncToHost);
        lastSyncToHost.store(syncToHost);
    }
    
    // Check sync rate changes
    if (syncRate != lastSyncRate.load() || firstRun.load()) {
        setSyncRate(syncRate);
        lastSyncRate.store(syncRate);
    }
    
    // Check waveshape changes
    if (waveshape != lastWaveshape.load() || firstRun.load()) {
        setWaveShape(waveshape);
        lastWaveshape.store(waveshape);
    }
    
    // Note: Coupling parameter removed from updateParameters as it should be set once during initialization
    
    // Mark first run as complete
    if (firstRun.load()) {
        firstRun.store(false);
    }
}

void LFO::updateFromPlayHead(juce::AudioPlayHead* playHead)
{
    if (playHead != nullptr) {
        juce::AudioPlayHead::CurrentPositionInfo positionInfo;
        if (playHead->getCurrentPosition(positionInfo)) {
            double hostBPM = positionInfo.bpm > 0.0 ? positionInfo.bpm : 120.0;
            bool isPlaying = positionInfo.isPlaying;
            
            // Use the extended version if we have beat position info
            if (positionInfo.ppqPositionOfLastBarStart >= 0.0) {
                double beatPosition = positionInfo.ppqPositionOfLastBarStart;
                double ppqPosition = positionInfo.ppqPosition;
                updateHostInfo(hostBPM, isPlaying, beatPosition, ppqPosition);
            } else {
                updateHostInfo(hostBPM, isPlaying);
            }
        }
    } else {
        // Fallback when no host available
        updateHostInfo(120.0, true);
    }
}

} // namespace audio_plugin 