#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::ephemeris {

class SunEquatorialCalculator final {
public:
    [[nodiscard]] core::EquatorialCoordinate compute(const core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
