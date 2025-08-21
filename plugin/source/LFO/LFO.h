#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

// Ensure M_PI is defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace audio_plugin {

/**
 * @brief A robust wavetable-based LFO for audio processing
 * 
 * This class provides a stable, thread-safe Low Frequency Oscillator with sine wave generation.
 * It uses a pre-calculated wavetable approach for consistent performance and eliminates
 * potential crashes through comprehensive bounds checking and safe initialization.
 * 
 * Thread Safety: Thread-safe for audio processing
 * Memory: Fixed-size wavetable allocated once during prepare()
 * 
 * Usage Example:
 * @code
 * LFO lfo;
 * lfo.prepare(48000.0);
 * lfo.setFrequency(1.0);  // 1 Hz
 * lfo.setDepth(0.8);      // 80% depth
 * 
 * // In processBlock:
 * float lfoValue = lfo.getNextSample();  // Returns [0, 1] * depth
 * @endcode
 */
class LFO {
public:
    /**
     * @brief Waveform types supported by the LFO
     */
    enum class WaveformType {
        Sine = 0,      ///< Sine wave (default)
        RampDown,      ///< Ramp down waveform
        RampUp,        ///< Ramp up waveform  
        Square,        ///< Square wave with rounded corners
        Triangle,      ///< Triangle wave
        HumpDown,      ///< U-shaped waveform (hump down)
        HumpUp         ///< Inverted U-shaped waveform (hump up)
    };

    /**
     * @brief Coupling types for LFO output
     */
    enum class CouplingType {
        DC = 0,        ///< DC coupling: output range [0, 1]
        AC = 1         ///< AC coupling: output range [-1, 1]
    };

    /**
     * @brief Default constructor
     */
    LFO();
    
    /**
     * @brief Destructor
     */
    ~LFO();

    /**
     * @brief Prepare the LFO for processing
     * @param sampleRate The sample rate in Hz
     * 
     * Must be called before processing any audio. This initializes the
     * wavetable and resets all state. Thread-safe to call multiple times.
     */
    void prepare(double sampleRate);

    /**
     * @brief Set the LFO frequency
     * @param frequencyInHz Frequency in Hz (will be clamped to valid range)
     * 
     * Updates the increment value for wavetable traversal.
     * Safe to call from any thread.
     */
    void setFrequency(double frequencyInHz);

    /**
     * @brief Set the modulation depth
     * @param depth Depth value (will be clamped to 0.0-1.0 range)
     * 
     * Controls the amplitude scaling of the LFO output.
     * Safe to call from any thread.
     */
    void setDepth(float depth);

    /**
     * @brief Set the phase offset
     * @param phaseOffsetInRadians Phase offset in radians
     * 
     * Adds a phase offset to the LFO for stereo effects or synchronization.
     * Safe to call from any thread.
     */
    void setPhaseOffset(double phaseOffsetInRadians);

    /**
     * @brief Set waveform inversion
     * @param shouldInvert True to invert the waveform (180° phase shift)
     * 
     * When inverted, the waveform is flipped while maintaining [0,1] range.
     * For example: 0.0 becomes 1.0, 0.75 becomes 0.25, etc.
     * Safe to call from any thread.
     */
    void setInvert(bool shouldInvert);

    /**
     * @brief Enable or disable the LFO
     * @param shouldEnable True to enable LFO processing, false to disable
     * 
     * When disabled, the LFO will output 0.0 regardless of other parameters.
     * This provides a convenient way to bypass the LFO without changing
     * other parameter settings.
     * Safe to call from any thread.
     */
    void setEnabled(bool shouldEnable);

    /**
     * @brief Set waveform symmetry
     * @param symmetryPercent Symmetry percentage (10.0 to 90.0)
     * 
     * Controls the time distribution of the waveform period:
     * - 50%: Normal symmetric waveform
     * - <50%: First half compressed, second half expanded
     * - >50%: First half expanded, second half compressed
     * Safe to call from any thread.
     */
    void setSymmetry(float symmetryPercent);

    /**
     * @brief Set the waveshape type
     * @param waveshape The waveshape type to use
     * 
     * Changes the LFO waveform shape. The wavetable will be regenerated
     * to match the selected waveshape while maintaining current symmetry settings.
     * Safe to call from any thread.
     */
    void setWaveShape(WaveformType waveshape);

    /**
     * @brief Set host sync mode
     * @param shouldSync True to sync to host tempo, false for manual frequency
     * 
     * When enabled, LFO frequency is calculated from host BPM and sync rate.
     * When disabled, uses manual frequency setting.
     * Safe to call from any thread.
     */
    void setSyncToHost(bool shouldSync);

    /**
     * @brief Set rhythm (musical division)
     * @param syncRateIndex Index of rhythm (0=1/2 Note, 1=1/4 Note, 2=1/4 Triplet, 3=1/8 Note, 4=1/8 Triplet, 5=1/16 Note)
     * 
     * Determines the musical division for host sync mode.
     * Safe to call from any thread.
     */
    void setSyncRate(int syncRateIndex);

    /**
     * @brief Set the coupling type for LFO output
     * @param coupling The coupling type (DC or AC)
     * 
     * DC coupling: output range [0, 1] (default)
     * AC coupling: output range [-1, 1] (bipolar)
     * Should only be called once during initialization, not during audio processing.
     * Safe to call from any thread.
     */
    void setCoupling(CouplingType coupling);

    /**
     * @brief Get the current coupling type
     * @return Current coupling type (DC or AC)
     * 
     * Safe to call from any thread.
     */
    CouplingType getCoupling() const;

    /**
     * @brief Update host tempo information
     * @param bpm Host BPM (beats per minute)
     * @param isPlaying Whether host transport is playing
     * 
     * Call this from processBlock to provide host timing information.
     * Safe to call from audio thread.
     */
    void updateHostInfo(double bpm, bool isPlaying);

    /**
     * @brief Update host tempo and beat position information
     * @param bpm Host BPM (beats per minute)
     * @param isPlaying Whether host transport is playing
     * @param beatPosition Current beat position (0.0 = downbeat, 1.0 = next downbeat)
     * @param ppqPosition Current PPQ (Pulses Per Quarter) position
     * 
     * Call this from processBlock to provide host timing and beat position information.
     * This enables downbeat locking when sync mode is active.
     * Safe to call from audio thread.
     */
    void updateHostInfo(double bpm, bool isPlaying, double beatPosition, double ppqPosition);

    /**
     * @brief Update all LFO parameters at once with automatic change detection
     * @param frequency LFO frequency in Hz
     * @param depth Modulation depth (0.0-1.0)
     * @param enabled Whether LFO is enabled
     * @param invert Whether to invert the waveform
     * @param phaseOffset Phase offset in degrees (-180 to 180)
     * @param symmetry Symmetry percentage (10.0-90.0)
     * @param syncToHost Whether to sync to host tempo
     * @param syncRate Rhythm index (0=1/2, 1=1/4, 2=1/4T, 3=1/8, 4=1/8T, 5=1/16)
     * @param waveshape Waveform type
     * 
     * This method efficiently updates all parameters and only regenerates
     * the wavetable when necessary. Call this from processBlock instead
     * of individual setter methods for optimal performance.
     * Note: Coupling type is not included as it should be set once during initialization.
     * Thread-safe for audio processing.
     */
    void updateParameters(float frequency, float depth, bool enabled,
                        bool invert, float phaseOffset, float symmetry, bool syncToHost,
                        int syncRate, WaveformType waveshape);

    /**
     * @brief Update host information and automatically handle playhead updates
     * @param playHead JUCE playhead pointer (can be nullptr)
     * 
     * This method automatically extracts host timing information and updates
     * the LFO accordingly. Call this from processBlock to keep the LFO
     * synchronized with the host.
     * Thread-safe for audio processing.
     */
    void updateFromPlayHead(juce::AudioPlayHead* playHead);

    /**
     * @brief Get the next LFO sample and advance position
     * @return LFO sample value normalized to [0, 1] range scaled by depth
     * 
     * This is the main processing method. Call once per sample to get
     * the LFO value and advance the internal position.
     * Thread-safe for audio processing.
     */
    float getNextSample();

    /**
     * @brief Get the current LFO sample without advancing position
     * @return Current LFO sample value
     * 
     * Returns the current LFO sample without advancing the position.
     * Thread-safe for audio processing.
     */
    float getCurrentSample() const;

    /**
     * @brief Reset the LFO phase to zero
     * 
     * Resets the internal position to 0, effectively restarting
     * the LFO cycle from the beginning.
     */
    void reset();

    /**
     * @brief Reset the LFO phase to a specific value
     * @param phaseInRadians Phase value in radians (0 to 2π)
     * 
     * Sets the internal position to the specified phase value.
     */
    void reset(double phaseInRadians);

    // Getters (all thread-safe)
    double getFrequency() const { return frequency.load(); }
    float getDepth() const { return smoothedDepth.getCurrentValue(); }
    double getPhaseOffset() const { return phaseOffset.load(); }
    WaveformType getWaveformType() const { return waveShape.load(); }
    bool isPrepared() const { return prepared.load(); }
    bool getInvert() const { return invert.load(); }
    float getSymmetry() const { return smoothedSymmetry.getCurrentValue(); }
    bool getSyncToHost() const { return syncToHost.load(); }
    int getSyncRate() const { return syncRateIndex.load(); }
    double getHostBPM() const { return hostBPM.load(); }
    double getSampleRate() const { return sampleRate.load(); }
    double getHostBeatPosition() const { return hostBeatPosition.load(); }
    double getHostPPQPosition() const { return hostPPQPosition.load(); }
    bool isDownbeatDetected() const { return downbeatDetected.load(); }
    bool isEnabled() const { return enabled.load(); }
    
    /**
     * @brief Get the current waveshape name as a string
     * @return String representation of the current waveshape
     */
    juce::String getWaveShapeName() const;
    
    // Position access for synchronization (thread-safe)
    float getPosition() const;
    void setPosition(float position);
    int getWaveTableSize() const;


private:

    // Internal helper methods
    void initializeWaveTable();
    void updateIncrement();
    float calculateSyncFrequency() const;
    void checkForDownbeatLock(double currentTime);
    
    // Waveshape generation methods
    void generateSineWave();
    void generateRampDownWave();
    void generateRampUpWave();
    void generateSquareWave();
    void generateTriangleWave();
    void generateHumpDownWave();
    void generateHumpUpWave();
    
    // Thread-safe parameters using atomics
    std::atomic<double> frequency{1.0};
    std::atomic<double> phaseOffset{0.0};
    std::atomic<double> sampleRate{44100.0};
    std::atomic<bool> prepared{false};
    std::atomic<bool> invert{false}; // Added for waveform inversion
    std::atomic<WaveformType> waveShape{WaveformType::Sine}; // Current waveshape
    std::atomic<CouplingType> coupling{CouplingType::DC}; // Output coupling type (set once at init)
    
    // Host sync parameters
    std::atomic<bool> syncToHost{false};
    std::atomic<int> syncRateIndex{1};  // Default to 1/4 note
    std::atomic<double> hostBPM{120.0};
    std::atomic<bool> hostIsPlaying{false};
    
    // Beat position tracking for downbeat locking
    std::atomic<double> hostBeatPosition{0.0};
    std::atomic<double> hostPPQPosition{0.0};
    std::atomic<double> lastDownbeatTime{0.0};
    std::atomic<bool> downbeatDetected{false};
    
    // Smoothed parameters to prevent clicks
    juce::SmoothedValue<float> smoothedDepth{0.0f};  // Start with 0 depth, let setDepth() set the actual value
    juce::SmoothedValue<float> smoothedSymmetry{0.5f};
    
    // Parameter change detection (for efficient updates)
    std::atomic<float> lastFrequency{-1.0f};
    std::atomic<float> lastDepth{-1.0f};
    std::atomic<bool> lastInvert{false};
    std::atomic<float> lastPhaseOffset{-999.0f};
    std::atomic<float> lastSymmetry{-1.0f};
    std::atomic<bool> lastSyncToHost{false};
    std::atomic<int> lastSyncRate{-1};
    std::atomic<WaveformType> lastWaveshape{WaveformType::Sine};
    std::atomic<bool> firstRun{true};
    
    // Enabled state
    std::atomic<bool> enabled{true};
    
    // Wavetable data (only modified during prepare())
    std::vector<float> waveTable;
    mutable std::atomic<float> position{0.0f};
    std::atomic<float> increment{0.0f};
    
    // Constants
    static constexpr double MIN_FREQUENCY = 0.001;   // Much lower minimum for very slow LFOs (like half notes)
    static constexpr double MAX_FREQUENCY = 1000.0;    // Higher maximum for flexibility
    static constexpr double TWO_PI = 2.0 * M_PI;
    static constexpr int DEFAULT_WAVETABLE_SIZE = 1024;  // Fixed size for predictability
    static constexpr float EPSILON = 1e-7f;         // Small value for comparisons
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFO)
};

} // namespace audio_plugin 