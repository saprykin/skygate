#pragma once

#include "EquatorialCoordinate.hpp"
#include "MoonEquatorialCalculator.hpp"
#include "PlanetEquatorialCalculator.hpp"
#include "SunEquatorialCalculator.hpp"
#include "UtcTimePoint.hpp"

#include <optional>

namespace skygate::ephemeris {

class BaseCelestialBody;

/// Computes the simple solar-system body equatorial state shared by the simple
/// engine and its fallback strategy. Both consumers reuse this component so
/// body support changes are expressed in exactly one place.
class SimpleBodyStateCalculator final {
public:
    [[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
    computeEquatorial(const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime) const noexcept;

private:
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
};

}  // namespace skygate::ephemeris
