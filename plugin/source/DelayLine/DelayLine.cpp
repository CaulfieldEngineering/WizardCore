#include "DelayLine.h"
#include "DigitalDelayLine/DigitalDelayLine.h"
#include "BBDelayLine/BBDelayLine.h"
#include <algorithm>
#include <cmath>

namespace audio_plugin {

std::unique_ptr<DelayLine> DelayLine::create(DelayType type)
{
    switch (type) {
        case DelayType::DigitalDelay:
            return createDigital();
        case DelayType::BBDelay:
            return createBBD();
        default:
            return createDigital(); // Fallback to digital delay
    }
}

std::unique_ptr<DelayLine> DelayLine::createDigital()
{
    return std::make_unique<DigitalDelayLine>();
}

std::unique_ptr<DelayLine> DelayLine::createBBD()
{
    return std::make_unique<BBDelayLine>();
}

std::unique_ptr<DelayLine> DelayLine::createBBD(int characteristic)
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

