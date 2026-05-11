#include "engine/SunEquatorialCalculator.hpp"

#include "engine/AstronomicalTime.hpp"
#include "engine/EclipticToEquatorialCalculator.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

core::EquatorialCoordinate SunEquatorialCalculator::compute(const core::UtcTimePoint& utcTime) const noexcept
{
    const double daysSinceJ2000 = AstronomicalTime::daysSinceJ2000(utcTime);
    const double meanLongitudeDeg = core::AngleMath::normalizeDegrees(
        280.460 + 0.9856474 * daysSinceJ2000
    );
    const double meanAnomalyDeg = core::AngleMath::normalizeDegrees(
        357.528 + 0.9856003 * daysSinceJ2000
    );
    const double meanAnomalyRad = core::AngleMath::toRadians(meanAnomalyDeg);

    const double eclipticLongitudeDeg = core::AngleMath::normalizeDegrees(
        meanLongitudeDeg + 1.915 * std::sin(meanAnomalyRad) + 0.020 * std::sin(2.0 * meanAnomalyRad)
    );

    return EclipticToEquatorialCalculator::compute(
        eclipticLongitudeDeg,
        0.0,
        AstronomicalTime::meanObliquityDeg(daysSinceJ2000)
    );
}

}  // namespace skygate::ephemeris
