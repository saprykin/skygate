#pragma once

#include "EquatorialCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class MoonEquatorialCalculator final {
public:
    [[nodiscard]] skygate::core::EquatorialCoordinate
    compute(const skygate::core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
