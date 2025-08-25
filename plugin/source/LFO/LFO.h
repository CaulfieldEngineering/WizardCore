#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

// Ensure M_PI maps to JUCE MathConstants for compatibility
#ifndef M_PI
#define M_PI juce::MathConstants<double>::pi
#endif

namespace audio_plugin {

/**
 * @brief Professional Low Frequency Oscillator (LFO) Module
 * 
 * A robust, thread-safe LFO with multiple waveform types, host synchronization,
 * and advanced modulation capabilities. Designed for professional audio applications
 * with emphasis on stability, performance, and flexibility.
 * 
 * Features:
 * - Multiple waveform types (Sine, Ramp, Square, Triangle, Hump)
 * - Host tempo synchronization with musical divisions
 * - Phase offset and symmetry control
 * - DC/AC coupling options
 * - Thread-safe parameter updates
 * - Efficient wavetable-based generation
 * 
 * Phase Offset Handling:
 * - Internal storage: All phase offsets are stored in radians for mathematical precision
 * - User interface: Methods available for both degrees and radians input/output
 * - Batch updates: updateParameters() expects radians for consistency
 * 
 * Sync Rhythm Handling:
 * - Musical divisions: Clear enum-based rhythm selection (1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16)
 * - No more magic numbers: Descriptive enum names instead of numeric indices
 * - Type safety: Compile-time validation of rhythm values
 * 
 * Thread Safety: Fully thread-safe for audio processing
 * Memory: Fixed-size wavetable allocated once during prepare()
 * 
 * Usage Example:
 * @code
 * LFO lfo;
 * lfo.prepare(48000.0);
 * lfo.setFrequency(2.0);      // 2 Hz
 * lfo.setDepth(0.75f);        // 75% depth
 * lfo.setWaveShape(LFO::WaveShape::Triangle);
 * 
 * // Phase offset options:
 * lfo.setPhaseOffsetDegrees(90.0f);     // 90 degrees
 * lfo.setPhaseOffsetRadians(M_PI/2.0);  // π/2 radians
 * 
 * // Sync rhythm options:
 * lfo.setSyncRhythm(LFO::SyncRhythm::QuarterNote);     // 1/4 note
 * lfo.setSyncRhythm(LFO::SyncRhythm::EighthTriplet);   // 1/8 triplet
 * 
 * // In processBlock:
 * float lfoValue = lfo.getNextSample();
 * @endcode
 */
class LFO {
public:
    // ============================================================================
    // ENUMERATIONS
    // ============================================================================
    
    /**
     * @brief Supported waveform types for LFO generation
     */
    enum class WaveShape {
        Sine = 0,          ///< Pure sine wave (default)
        RampDown,          ///< Descending ramp with rounded corners
        RampUp,            ///< Ascending ramp with rounded corners
        Square,            ///< Square wave with smooth transitions
        Triangle,          ///< Linear triangle wave
        HumpDown,          ///< U-shaped waveform (hump down)
        HumpUp             ///< Inverted U-shaped waveform (hump up)
    };

    /**
     * @brief Output coupling types for LFO signal
     */
    enum class CouplingType {
        DC = 0,            ///< DC coupling: output range [0, 1]
        AC = 1             ///< AC coupling: output range [-1, 1]
    };

    /**
     * @brief Musical rhythm divisions for host synchronization
     */
    enum class SyncRhythm {
        HalfNote = 0,      ///< 1/2 Note (2 beats per cycle)
        QuarterNote = 1,   ///< 1/4 Note (1 beat per cycle)
        QuarterTriplet = 2,///< 1/4 Triplet (1.333 beats per cycle)
        EighthNote = 3,    ///< 1/8 Note (0.5 beats per cycle)
        EighthTriplet = 4, ///< 1/8 Triplet (0.667 beats per cycle)
        SixteenthNote = 5  ///< 1/16 Note (0.25 beats per cycle)
    };

    // ============================================================================
    // CONFIGURATION STRUCT
    // ============================================================================
    
    /**
     * @brief Configuration struct containing all LFO parameters of sonic/usable interest
     * 
     * This struct contains the parameters that define how the LFO sounds and behaves.
     * It is public so external code can access the parameter structure for debugging
     * and monitoring, but the actual config instance remains private.
     */
    struct Config {
        std::atomic<double> frequencyHz{1.0};           ///< LFO frequency in Hz
        std::atomic<float> depth{1.0f};                 ///< Modulation depth [0.0, 1.0]
        std::atomic<double> phaseOffsetRadians{0.0};    ///< Phase offset in radians
        std::atomic<float> symmetry{0.5f};              ///< Symmetry value [0.1, 0.9]
        std::atomic<bool> enabled{true};                ///< Enabled state
        std::atomic<bool> invert{false};                ///< Inversion state
        std::atomic<bool> syncToHost{false};            ///< Host sync state
        std::atomic<SyncRhythm> syncRhythm{SyncRhythm::QuarterNote}; ///< Sync rhythm
        std::atomic<WaveShape> waveShape{WaveShape::Sine}; ///< Waveform type
        std::atomic<CouplingType> coupling{CouplingType::DC}; ///< Coupling type
    };

    // ============================================================================
    // CONSTRUCTOR & DESTRUCTOR
    // ============================================================================
    
    /**
     * @brief Default constructor
     * 
     * Initializes the LFO with default configuration values.
     * Call prepare() before processing audio.
     */
    LFO();
    
    /**
     * @brief Destructor
     */
    ~LFO();

    // ============================================================================
    // INITIALIZATION & SETUP
    // ============================================================================
    
    /**
     * @brief Prepare the LFO for audio processing
     * @param sampleRate The audio sample rate in Hz
     * 
     * Must be called before processing any audio. This method:
     * - Initializes the wavetable with current waveform settings
     * - Sets up parameter smoothing
     * - Resets internal state
     * - Marks the LFO as ready for processing
     * 
     * Thread-safe and can be called multiple times.
     */
    void prepare(double sampleRate);

    // ============================================================================
    // INDIVIDUAL PARAMETER SETTERS
    // ============================================================================
    
    /**
     * @brief Set the LFO frequency
     * @param frequencyInHz Frequency in Hz (clamped to valid range)
     * 
     * Updates the increment value for wavetable traversal.
     * Valid range: 0.001 Hz to 1000 Hz
     * Thread-safe for audio processing.
     */
    void setFrequency(double frequencyInHz);

    /**
     * @brief Set the modulation depth
     * @param depth Depth value [0.0, 1.0] (clamped to valid range)
     * 
     * Controls the amplitude scaling of the LFO output.
     * Thread-safe for audio processing.
     */
    void setDepth(float depth);

    /**
     * @brief Set the phase offset in radians
     * @param phaseOffsetInRadians Phase offset in radians
     * 
     * Adds a phase offset to the LFO for stereo effects or synchronization.
     * Thread-safe for audio processing.
     */
    void setPhaseOffsetRadians(double phaseOffsetInRadians);

    /**
     * @brief Set the phase offset in degrees
     * @param phaseOffsetInDegrees Phase offset in degrees [-180, 180]
     * 
     * Adds a phase offset to the LFO for stereo effects or synchronization.
     * Values are automatically clamped to the valid range [-180, 180] degrees.
     * Thread-safe for audio processing.
     */
    void setPhaseOffsetDegrees(float phaseOffsetInDegrees);

    /**
     * @brief Set waveform inversion
     * @param shouldInvert True to invert the waveform (180° phase shift)
     * 
     * When inverted, the waveform is flipped while maintaining [0,1] range.
     * Thread-safe for audio processing.
     */
    void setInvert(bool shouldInvert);

    /**
     * @brief Enable or disable the LFO
     * @param shouldEnable True to enable LFO processing, false to disable
     * 
     * When disabled, the LFO outputs 1.0 (no modulation).
     * Thread-safe for audio processing.
     */
    void setEnabled(bool shouldEnable);

    /**
     * @brief Set waveform symmetry
     * @param symmetryValue Symmetry value [0.1, 0.9] or [10.0, 90.0]
     * 
     * Controls the time distribution of the waveform period:
     * - 0.5: Normal symmetric waveform
     * - <0.5: First half compressed, second half expanded
     * - >0.5: First half expanded, second half compressed
     * Thread-safe for audio processing.
     */
    void setSymmetry(float symmetryValue);

    /**
     * @brief Set the waveform type
     * @param waveshape The waveshape type to use
     * 
     * Changes the LFO waveform shape. The wavetable is regenerated
     * to match the selected waveshape while maintaining current symmetry settings.
     * Thread-safe for audio processing.
     */
    void setWaveShape(WaveShape waveshape);

    /**
     * @brief Set host synchronization mode
     * @param shouldSync True to sync to host tempo, false for manual frequency
     * 
     * When enabled, LFO frequency is calculated from host BPM and sync rate.
     * When disabled, uses manual frequency setting.
     * Thread-safe for audio processing.
     */
    void setSyncToHost(bool shouldSync);

    /**
     * @brief Set the rhythm (musical division) for host sync
     * @param rhythm The musical rhythm division to use
     * 
     * Determines the musical division for host sync mode.
     * Thread-safe for audio processing.
     */
    void setSyncRhythm(SyncRhythm rhythm);

    /**
     * @brief Set the coupling type for LFO output
     * @param coupling The coupling type (DC or AC)
     * 
     * Should only be called once during initialization, not during audio processing.
     * Thread-safe for audio processing.
     */
    void setCoupling(CouplingType coupling);

    // ============================================================================
    // BATCH PARAMETER UPDATES
    // ============================================================================
    
    /**
     * @brief Update all LFO parameters at once with automatic change detection
     * @param frequency LFO frequency in Hz
     * @param depth Modulation depth [0.0, 1.0]
     * @param enabled Whether LFO is enabled
     * @param invert Whether to invert the waveform
     * @param phaseOffset Phase offset in radians
     * @param symmetry Symmetry percentage [10.0, 90.0]
     * @param syncToHost Whether to sync to host tempo
     * @param syncRhythm Musical rhythm division for host sync
     * @param waveshape Waveform type
     * 
     * This method efficiently updates all parameters and only regenerates
     * the wavetable when necessary. Call this from processBlock instead
     * of individual setter methods for optimal performance.
     * 
     * Note: Coupling type is not included as it should be set once during initialization.
     * Thread-safe for audio processing.
     */
    void updateParameters(float frequency, float depth, bool enabled,
                        bool invert, float phaseOffset, float symmetry, bool syncToHost,
                        SyncRhythm syncRhythm, WaveShape waveshape);

    // ============================================================================
    // HOST SYNCHRONIZATION
    // ============================================================================
    
    /**
     * @brief Update host tempo information
     * @param bpm Host BPM (beats per minute)
     * @param isPlaying Whether host transport is playing
     * 
     * Call this from processBlock to provide host timing information.
     * Thread-safe for audio processing.
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
     * Thread-safe for audio processing.
     */
    void updateHostInfo(double bpm, bool isPlaying, double beatPosition, double ppqPosition);

    /**
     * @brief Update host information from JUCE playhead
     * @param playHead JUCE playhead pointer (can be nullptr)
     * 
     * This method automatically extracts host timing information and updates
     * the LFO accordingly. Call this from processBlock to keep the LFO
     * synchronized with the host.
     * Thread-safe for audio processing.
     */
    void updateFromPlayHead(juce::AudioPlayHead* playHead);

    // ============================================================================
    // AUDIO PROCESSING
    // ============================================================================
    
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

    // ============================================================================
    // STATE CONTROL
    // ============================================================================
    
    /**
     * @brief Reset the LFO phase to zero
     * 
     * Resets the internal position to 0, effectively restarting
     * the LFO cycle from the beginning.
     */
    void reset();

    /**
     * @brief Reset the LFO phase to a specific value
     * @param phaseInRadians Phase value in radians [0, 2π]
     * 
     * Sets the internal position to the specified phase value.
     */
    void reset(double phaseInRadians);

    // ============================================================================
    // GETTERS (ALL THREAD-SAFE)
    // ============================================================================
    
    // Configuration access for debugging/monitoring
    const Config& getConfig() const { return config; }
    
    // Core parameters
    double getFrequency() const { return config.frequencyHz.load(); }
    float getDepth() const { return smoothedDepth.getCurrentValue(); }
    
    // Phase offset getters - choose based on your preferred units
    double getPhaseOffsetRadians() const { return config.phaseOffsetRadians.load(); } // Returns radians (explicit)
    float getPhaseOffsetDegrees() const { return config.phaseOffsetRadians.load() * 180.0 / M_PI; } // Returns degrees
    
    WaveShape getWaveShape() const { return config.waveShape.load(); }
    bool isPrepared() const { return prepared.load(); }
    bool getInvert() const { return config.invert.load(); }
    float getSymmetry() const { return smoothedSymmetry.getCurrentValue(); }     // Returns symmetry [0.1, 0.9]
    
    // Host sync parameters
    bool getSyncToHost() const { return config.syncToHost.load(); }
    SyncRhythm getSyncRhythm() const { return config.syncRhythm.load(); }
    double getHostBPM() const { return hostBPM.load(); }                         // Returns host BPM
    double getSampleRate() const { return sampleRateHz.load(); }                   // Returns sample rate in Hz
    double getHostBeatPosition() const { return hostBeatPosition.load(); }       // Returns beat position [0.0, 1.0]
    double getHostPPQPosition() const { return hostPPQPosition.load(); }        // Returns PPQ position
    bool isDownbeatDetected() const { return downbeatDetected.load(); }
    
    // State parameters
    bool isEnabled() const { return config.enabled.load(); }
    CouplingType getCoupling() const { return config.coupling.load(); }
    
    // Utility getters
    juce::String getWaveShapeName() const;
    juce::String getSyncRhythmName() const;
    float getPosition() const;                                                  // Returns wavetable position [0, tableSize)
    void setPosition(float position);
    int getWaveTableSize() const;                                              // Returns wavetable size

private:
    // ============================================================================
    // PRIVATE CONSTANTS
    // ============================================================================
    
    static constexpr double MIN_FREQUENCY_HZ = 0.001;           ///< Minimum LFO frequency in Hz
    static constexpr double MAX_FREQUENCY_HZ = 1000.0;          ///< Maximum LFO frequency in Hz
    static constexpr double TWO_PI = juce::MathConstants<double>::twoPi; ///< 2π constant
    static constexpr int DEFAULT_WAVETABLE_SIZE = 1024;         ///< Fixed wavetable size for predictability
    static constexpr float EPSILON = 1e-7f;                    ///< Small value for floating-point comparisons
    static constexpr float DEFAULT_SMOOTHING_TIME_SECONDS = 0.05f; ///< Default parameter smoothing time (50ms)
    static constexpr double DOWNBEAT_TOLERANCE_BEATS = 0.1;     ///< Tolerance for downbeat detection in beats
    static constexpr double DOWNBEAT_TIME_TOLERANCE_SECONDS = 0.1; ///< Minimum time between downbeat detections

    // ============================================================================
    // PRIVATE HELPER METHODS
    // ============================================================================
    
    // Wavetable management
    void initializeWaveTable();
    void updateIncrement();
    
    // Host sync calculations
    float calculateSyncFrequency() const;
    void checkForDownbeatLock(double currentTime);
    
    // Waveform generation methods
    void generateSineWave();
    void generateRampDownWave();
    void generateRampUpWave();
    void generateSquareWave();
    void generateTriangleWave();
    void generateHumpDownWave();
    void generateHumpUpWave();



    // ============================================================================
    // PRIVATE MEMBER VARIABLES
    // ============================================================================
    
    // Configuration struct containing all LFO parameters
    Config config;
    
    // Processing state variables (not part of config)
    std::atomic<bool> prepared{false};
    std::atomic<bool> hostIsPlaying{false};
    std::atomic<double> hostBPM{120.0};
    std::atomic<double> sampleRateHz{44100.0};
    std::atomic<double> hostBeatPosition{0.0};
    std::atomic<double> hostPPQPosition{0.0};
    std::atomic<double> lastDownbeatTime{0.0};
    std::atomic<bool> downbeatDetected{false};
    
    // Smoothed parameters to prevent audio clicks
    juce::SmoothedValue<float> smoothedDepth{config.depth.load()};
    juce::SmoothedValue<float> smoothedSymmetry{config.symmetry.load()};
    
    // Parameter change detection for efficient updates
    std::atomic<float> lastFrequency{-1.0f};
    std::atomic<float> lastDepth{-1.0f};
    std::atomic<bool> lastInvert{false};
    std::atomic<float> lastPhaseOffsetInRadians{-999.0f}; ///< Last phase offset value in radians for change detection
    std::atomic<float> lastSymmetry{-1.0f};
    std::atomic<bool> lastSyncToHost{false};
    std::atomic<SyncRhythm> lastSyncRhythm{SyncRhythm::QuarterNote};
    std::atomic<WaveShape> lastWaveshape{WaveShape::Sine};
    std::atomic<bool> firstRun{true};
    
    // Wavetable data (only modified during prepare())
    std::vector<float> waveTable;
    mutable std::atomic<float> position{0.0f};
    std::atomic<float> increment{0.0f};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFO)
};

} // namespace audio_plugin
