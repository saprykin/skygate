#pragma once

#include "EquatorialCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class SunEquatorialCalculator final {
public:
    [[nodiscard]] skygate::core::EquatorialCoordinate
    compute(const skygate::core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
