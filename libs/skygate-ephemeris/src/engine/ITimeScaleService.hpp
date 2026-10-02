#pragma once

#include "TimeScaleConversionResult.hpp"
#include "time/CivilDateTime.hpp"

namespace skygate::ephemeris {

class ITimeScaleService {
public:
    virtual ~ITimeScaleService() = default;

    [[nodiscard]] virtual TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale) const = 0;

    [[nodiscard]] virtual TimeScaleConversionResult
    convertCivilDateTime(const skygate::core::CivilDateTime& dateTime, skygate::core::TimeScale targetScale) const = 0;
};

}  // namespace skygate::ephemeris
