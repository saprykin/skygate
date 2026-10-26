#pragma once

#include "TimeScaleConversionResult.hpp"

namespace skygate::ephemeris {

class TdbTtConverter final {
public:
    [[nodiscard]] static TimeScaleConversionResult ttToTdb(const skygate::core::AstronomicalEpoch& terrestrialTime);
    [[nodiscard]] static TimeScaleConversionResult tdbToTt(const skygate::core::AstronomicalEpoch& tdbEpoch);
};

}  // namespace skygate::ephemeris
