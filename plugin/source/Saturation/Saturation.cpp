#include "Saturation.h"
#include <cmath>
#include <algorithm>

namespace WizardCore {

    // Constructor
    // ==============================================================================

    Saturation::Saturation() {}

    // prepare
    // ==============================================================================

    void Saturation::prepare(double newSampleRate, int newNumChannels) {
        sampleRate = newSampleRate;
        numChannels = newNumChannels;
        toneFilterState.resize(newNumChannels, 0.0f);
        std::fill(toneFilterState.begin(), toneFilterState.end(), 0.0f);
        prepared.store(true);
    }

    // processBlock
    // ==============================================================================

    void Saturation::processBlock(juce::AudioBuffer<float>& buffer) {
        if (!config.enabled.load())
            return;

        const int numSamples = buffer.getNumSamples();
        const int channels = std::min(buffer.getNumChannels(), numChannels);
        const float driveAmount = config.drive.load();
        const float toneParam = config.tone.load();
        const float mixAmount = config.mix.load();
        const float outGainDb = config.outputGain.load();
        const float outGainLinear = juce::Decibels::decibelsToGain(outGainDb);
        const Type satType = config.type.load();

        // Tone filter coefficient — one-pole tilt
        // At tone=0.5 the filter is neutral. Below darkens, above brightens.
        const float toneCoeff = std::clamp((toneParam - 0.5f) * 2.0f, -1.0f, 1.0f);
        const float filterAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi *
            (3000.0f + toneCoeff * 2500.0f) / static_cast<float>(sampleRate));

        for (int ch = 0; ch < channels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            float& filterState = toneFilterState[ch];

            for (int i = 0; i < numSamples; ++i) {
                const float dry = data[i];

                // Apply saturation based on type
                float wet;
                switch (satType) {
                    case Type::Tape:      wet = saturateTape(dry, driveAmount); break;
                    case Type::Tube:      wet = saturateTube(dry, driveAmount); break;
                    case Type::Overdrive: wet = saturateOverdrive(dry, driveAmount); break;
                    default:              wet = saturateTape(dry, driveAmount); break;
                }

                // Tone filter — tilt EQ
                // Positive toneCoeff boosts highs (subtract lowpass), negative boosts lows (add lowpass)
                filterState += filterAlpha * (wet - filterState);
                if (toneCoeff > 0.0f)
                    wet = wet + toneCoeff * (wet - filterState);  // boost highs
                else
                    wet = wet + toneCoeff * (filterState - wet);  // boost lows (subtract highs)

                // Output gain
                wet *= outGainLinear;

                // Dry/wet mix
                data[i] = dry * (1.0f - mixAmount) + wet * mixAmount;
            }
        }
    }

    // processSample
    // ==============================================================================

    void Saturation::processSample(float& left, float& right) {
        if (!config.enabled.load())
            return;

        const float driveAmount = config.drive.load();
        const Type satType = config.type.load();

        switch (satType) {
            case Type::Tape:
                left = saturateTape(left, driveAmount);
                right = saturateTape(right, driveAmount);
                break;
            case Type::Tube:
                left = saturateTube(left, driveAmount);
                right = saturateTube(right, driveAmount);
                break;
            case Type::Overdrive:
                left = saturateOverdrive(left, driveAmount);
                right = saturateOverdrive(right, driveAmount);
                break;
        }
    }

    // clear
    // ==============================================================================

    void Saturation::clear() {
        std::fill(toneFilterState.begin(), toneFilterState.end(), 0.0f);
    }

    // setEnabled
    // ==============================================================================

    void Saturation::setEnabled(bool enabled) {
        config.enabled.store(enabled);
    }

    // setType
    // ==============================================================================

    void Saturation::setType(Type type) {
        config.type.store(type);
    }

    // setDrive
    // ==============================================================================

    void Saturation::setDrive(float drive) {
        config.drive.store(std::clamp(drive, 0.0f, 1.0f));
    }

    // setTone
    // ==============================================================================

    void Saturation::setTone(float tone) {
        config.tone.store(std::clamp(tone, 0.0f, 1.0f));
    }

    // setMix
    // ==============================================================================

    void Saturation::setMix(float mix) {
        config.mix.store(std::clamp(mix, 0.0f, 1.0f));
    }

    // setOutputGain
    // ==============================================================================

    void Saturation::setOutputGain(float gainDb) {
        config.outputGain.store(std::clamp(gainDb, -12.0f, 12.0f));
    }

    // saturateTape
    // ==============================================================================
    // Models magnetic tape saturation:
    //   - Soft symmetric clipping via tanh
    //   - Slight bass bump from tape head proximity effect
    //   - High frequencies compress first (tape self-erasure)

    float Saturation::saturateTape(float input, float driveAmount) {
        // Drive maps 0-1 to 1x-6x gain into the curve
        const float drive = 1.0f + driveAmount * 5.0f;
        float driven = input * drive;

        // Tape saturation — tanh with gain compensation
        float saturated = std::tanh(driven) / std::tanh(drive);

        // Subtle bass emphasis (tape head bump) — more at higher drive
        // Mix in a tiny bit of the original low-frequency content
        float bassBoost = 1.0f + driveAmount * 0.08f;
        saturated *= bassBoost;

        return saturated;
    }

    // saturateTube
    // ==============================================================================
    // Models tube amplifier saturation:
    //   - Asymmetric clipping (positive and negative halves differ)
    //   - Adds even harmonics (warmth) alongside odd harmonics
    //   - Gentle compression on the positive half, harder on negative

    float Saturation::saturateTube(float input, float driveAmount) {
        const float drive = 1.0f + driveAmount * 5.0f;
        float driven = input * drive;

        // Asymmetric waveshaping — positive half clips softer than negative
        float saturated;
        if (driven >= 0.0f) {
            // Positive: gentle soft clip (tube conducts smoothly on positive swing)
            saturated = std::tanh(driven * 0.8f);
        } else {
            // Negative: harder clip (tube cuts off more abruptly)
            saturated = std::tanh(driven * 1.2f);
        }

        // Gain compensation
        saturated /= std::tanh(drive * 0.8f);

        return saturated;
    }

    // saturateOverdrive
    // ==============================================================================
    // Models transistor overdrive:
    //   - Harder clipping than tape or tube
    //   - Symmetric, mostly odd harmonics
    //   - More aggressive compression

    float Saturation::saturateOverdrive(float input, float driveAmount) {
        const float drive = 1.0f + driveAmount * 8.0f;
        float driven = input * drive;

        // Hard-ish symmetric clipping — cubic soft clip transitioning to hard clip
        float saturated;
        if (std::abs(driven) < 1.0f) {
            // Cubic soft clip region
            saturated = driven - (driven * driven * driven) / 3.0f;
        } else {
            // Hard clip beyond threshold
            saturated = std::copysign(2.0f / 3.0f, driven);
        }

        // Gain compensation
        saturated *= 1.5f;

        return saturated;
    }

} // namespace WizardCore
