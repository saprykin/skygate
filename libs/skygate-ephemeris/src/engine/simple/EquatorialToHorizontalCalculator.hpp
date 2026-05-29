#pragma once

#include "EquatorialCoordinate.hpp"
#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class EquatorialToHorizontalCalculator final {
public:
    [[nodiscard]] static skygate::core::HorizontalCoordinate compute(
        const skygate::core::EquatorialCoordinate& equatorial,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;
};

}  // namespace skygate::ephemeris
