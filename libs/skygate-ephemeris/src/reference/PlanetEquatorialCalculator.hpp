#pragma once

#include "EquatorialCoordinate.hpp"
#include "UtcTimePoint.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class PlanetEquatorialCalculator final {
public:
    [[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
    compute(std::string_view bodyId, const skygate::core::UtcTimePoint& utcTime) const noexcept;
};

}  // namespace skygate::ephemeris
