#include "DelayLineFactory.h"
#include "../BBDelayLine/BBDelayLine.h"

namespace audio_plugin {

std::unique_ptr<DelayLine> DelayLineFactory::createDelayLine(DelayType type)
{
    switch (type) {
        case DelayType::DigitalDelay:
            return createDigitalDelay();
        case DelayType::BBDelay:
            return createBBDelay();
        default:
            return createDigitalDelay(); // Fallback to digital delay
    }
}

std::unique_ptr<DelayLine> DelayLineFactory::createDigitalDelay()
{
    return std::make_unique<DelayLine>();
}

std::unique_ptr<DelayLine> DelayLineFactory::createBBDelay()
{
    return std::make_unique<BBDelayLine>();
}

std::unique_ptr<DelayLine> DelayLineFactory::createBBDelay(int characteristic)
{
    auto bbDelay = std::make_unique<BBDelayLine>();
    // Convert integer to BBDCharacteristic enum
    BBDelayLine::BBDCharacteristic bbChar;
    switch (characteristic) {
        case 0: bbChar = BBDelayLine::BBDCharacteristic::Vintage; break;
        case 1: bbChar = BBDelayLine::BBDCharacteristic::Modern; break;
        case 2: bbChar = BBDelayLine::BBDCharacteristic::Dirty; break;
        default: bbChar = BBDelayLine::BBDCharacteristic::Vintage; break;
    }
    bbDelay->setBBDCharacteristic(bbChar);
    return bbDelay;
}

} // namespace audio_plugin 