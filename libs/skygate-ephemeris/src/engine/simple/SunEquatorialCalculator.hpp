#pragma once

#include "skygate/core/EquatorialCoordinate.hpp"
#include "skygate/core/UtcTimePoint.hpp"

namespace skygate::ephemeris {

class SunEquatorialCalculator final {
public:
    [[nodiscard]] core::EquatorialCoordinate compute(const core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
