#include "DelayLine.h"
#include "DigitalDelayLine/DigitalDelayLine.h"
#include "BBDelayLine/BBDelayLine.h"
#include <algorithm>
#include <cmath>

namespace WizardCore {

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



} // namespace WizardCore

