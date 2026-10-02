#pragma once

#include "TimeScaleConversionResult.hpp"

namespace skygate::ephemeris {

class TdbTtConverter final {
public:
    [[nodiscard]] static TimeScaleConversionResult ttToTdb(const AstronomicalEpoch& terrestrialTime);
    [[nodiscard]] static TimeScaleConversionResult tdbToTt(const AstronomicalEpoch& tdbEpoch);
};

}  // namespace skygate::ephemeris
