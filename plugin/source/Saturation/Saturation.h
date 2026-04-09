#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace WizardCore {

/**
 * @brief Saturation / overdrive module with multiple character modes
 *
 * Provides harmonic saturation with switchable algorithms modeled after
 * real analog circuits: tape machines, tube amplifiers, and transistor
 * overdrive stages. Includes optional tone shaping and oversampling.
 *
 * Thread Safety: Thread-safe for audio processing
 * Memory: Minimal — no dynamic allocation after prepare()
 */
class Saturation {

public:

    // Enums
    // ==============================================================================

    enum class Type {
        Tape,       ///< Tape saturation — soft, warm, slight bass emphasis
        Tube,       ///< Tube saturation — asymmetric, even + odd harmonics
        Overdrive   ///< Transistor overdrive — harder clipping, more aggressive
    };

    // Config
    // ==============================================================================

    struct Config {
        std::atomic<bool> enabled{true};
        std::atomic<Type> type{Type::Tape};
        std::atomic<float> drive{0.5f};         ///< Input drive [0.0, 1.0]
        std::atomic<float> tone{0.5f};          ///< Post-saturation tilt EQ [0.0, 1.0] (0=dark, 1=bright)
        std::atomic<float> mix{1.0f};           ///< Dry/wet blend [0.0, 1.0]
        std::atomic<float> outputGain{0.0f};    ///< Output gain in dB [-12, +12]
    };

    // Constructor & Destructor
    // ==============================================================================

    Saturation();
    ~Saturation() = default;

    // Public Interface
    // ==============================================================================

    const Config& getConfig() const { return config; }

    // Audio Lifecycle
    // ==============================================================================

    void prepare(double sampleRate, int numChannels);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void processSample(float& left, float& right);
    void clear();
    bool isPrepared() const { return prepared.load(); }

    // Setters
    // ==============================================================================

    void setEnabled(bool enabled);
    void setType(Type type);
    void setDrive(float drive);
    void setTone(float tone);
    void setMix(float mix);
    void setOutputGain(float gainDb);

    // Getters
    // ==============================================================================

    bool isEnabled() const { return config.enabled.load(); }
    Type getType() const { return config.type.load(); }
    float getDrive() const { return config.drive.load(); }
    float getTone() const { return config.tone.load(); }
    float getMix() const { return config.mix.load(); }
    float getOutputGain() const { return config.outputGain.load(); }

private:

    // Config
    // ==============================================================================

    Config config;

    // System State
    // ==============================================================================

    std::atomic<bool> prepared{false};
    double sampleRate{44100.0};
    int numChannels{2};

    // Tone Filter (simple one-pole tilt EQ per channel)
    // ==============================================================================

    std::vector<float> toneFilterState;

    // Internal Processing
    // ==============================================================================

    float saturateTape(float input, float driveAmount);
    float saturateTube(float input, float driveAmount);
    float saturateOverdrive(float input, float driveAmount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Saturation)
};

} // namespace WizardCore
