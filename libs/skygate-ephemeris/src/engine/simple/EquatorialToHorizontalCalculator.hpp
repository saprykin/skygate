#pragma once

#include "EquatorialCoordinate.hpp"
#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class EquatorialToHorizontalCalculator final {
public:
    [[nodiscard]] static core::HorizontalCoordinate compute(
        const core::EquatorialCoordinate& equatorial,
        const core::GeoLocation& observer,
        const core::UtcTimePoint& utcTime
    ) noexcept;
};

}  // namespace skygate::ephemeris
