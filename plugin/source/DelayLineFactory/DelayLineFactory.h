#pragma once

#include "../DelayLine/DelayLine.h"
#include <memory>

namespace audio_plugin {

/**
 * @brief Available delay line types
 */
enum class DelayType {
    DigitalDelay,    ///< Clean digital delay (DelayLine)
    BBDelay          ///< Bucket Brigade Delay emulation (BBDelayLine)
};

/**
 * @brief Factory for creating different types of delay lines
 * 
 * This factory allows creating different delay line types while maintaining
 * the same interface. This enables seamless switching between delay types
 * in effects like Chorus without changing any processing code.
 */
class DelayLineFactory {
public:
    /**
     * @brief Create a delay line instance of the specified type
     * @param type The type of delay line to create
     * @return Unique pointer to the created delay line
     */
    static std::unique_ptr<DelayLine> createDelayLine(DelayType type = DelayType::BBDelay);
    
    /**
     * @brief Create a clean digital delay line
     * @return Unique pointer to a DelayLine instance
     */
    static std::unique_ptr<DelayLine> createDigitalDelay();
    
    /**
     * @brief Create a BBD delay line with vintage characteristics
     * @return Unique pointer to a BBDelayLine instance
     */
    static std::unique_ptr<DelayLine> createBBDelay();
    
    /**
     * @brief Create a BBD delay line with specified characteristics
     * @param characteristic The BBD characteristic to use (0=Vintage, 1=Modern, 2=Dirty)
     * @return Unique pointer to a BBDelayLine instance
     */
    static std::unique_ptr<DelayLine> createBBDelay(int characteristic);
};

} // namespace audio_plugin 