#pragma once

#include "EquatorialCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class SunEquatorialCalculator final {
public:
    [[nodiscard]] core::EquatorialCoordinate compute(const core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
