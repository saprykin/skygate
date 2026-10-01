#pragma once

#include "TimeScaleConversionResult.hpp"
#include "time/CivilDateTime.hpp"

namespace skygate::ephemeris {

class ITimeScaleService {
public:
    virtual ~ITimeScaleService() = default;

    [[nodiscard]] virtual TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, TimeScale targetScale) const = 0;

    [[nodiscard]] virtual TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, TimeScale targetScale) const = 0;
};

}  // namespace skygate::ephemeris
