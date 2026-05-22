#pragma once

#include "skygate/core/EquatorialCoordinate.hpp"
#include "skygate/core/UtcTimePoint.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class PlanetEquatorialCalculator final {
public:
    [[nodiscard]] std::optional<core::EquatorialCoordinate>
    compute(std::string_view bodyId, const core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
