#include "engine/MoonEquatorialCalculator.hpp"

#include "engine/AstronomicalTime.hpp"
#include "engine/EclipticToEquatorialCalculator.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

core::EquatorialCoordinate MoonEquatorialCalculator::compute(const core::UtcTimePoint& utcTime) const noexcept
{
    const double daysSinceJ2000 = AstronomicalTime::daysSinceJ2000(utcTime);
    const double ascendingNodeDeg = core::AngleMath::normalizeDegrees(
        125.1228 - 0.0529538083 * daysSinceJ2000
    );
    const double inclinationDeg = 5.1454;
    const double argumentOfPerigeeDeg = core::AngleMath::normalizeDegrees(
        318.0634 + 0.1643573223 * daysSinceJ2000
    );
    const double eccentricity = 0.0549;
    const double meanAnomalyDeg = core::AngleMath::normalizeDegrees(
        115.3654 + 13.0649929509 * daysSinceJ2000
    );

    const double meanAnomalyRad = core::AngleMath::toRadians(meanAnomalyDeg);
    const double eccentricAnomalyDeg = meanAnomalyDeg + core::AngleMath::toDegrees(
        eccentricity * std::sin(meanAnomalyRad) * (1.0 + eccentricity * std::cos(meanAnomalyRad))
    );
    const double eccentricAnomalyRad = core::AngleMath::toRadians(eccentricAnomalyDeg);

    constexpr double kSemiMajorAxisEarthRadii = 60.2666;
    const double xV = kSemiMajorAxisEarthRadii * (std::cos(eccentricAnomalyRad) - eccentricity);
    const double yV = kSemiMajorAxisEarthRadii * (
        std::sqrt(1.0 - eccentricity * eccentricity) * std::sin(eccentricAnomalyRad)
    );

    const double trueAnomalyDeg = core::AngleMath::toDegrees(std::atan2(yV, xV));
    const double radius = std::sqrt(xV * xV + yV * yV);

    const double longitudeArgumentRad = core::AngleMath::toRadians(trueAnomalyDeg + argumentOfPerigeeDeg);
    const double ascendingNodeRad = core::AngleMath::toRadians(ascendingNodeDeg);
    const double inclinationRad = core::AngleMath::toRadians(inclinationDeg);

    const double xH = radius * (
        std::cos(ascendingNodeRad) * std::cos(longitudeArgumentRad)
        - std::sin(ascendingNodeRad) * std::sin(longitudeArgumentRad) * std::cos(inclinationRad)
    );
    const double yH = radius * (
        std::sin(ascendingNodeRad) * std::cos(longitudeArgumentRad)
        + std::cos(ascendingNodeRad) * std::sin(longitudeArgumentRad) * std::cos(inclinationRad)
    );
    const double zH = radius * std::sin(longitudeArgumentRad) * std::sin(inclinationRad);

    const double eclipticLongitudeDeg = core::AngleMath::normalizeDegrees(
        core::AngleMath::toDegrees(std::atan2(yH, xH))
    );
    const double eclipticLatitudeDeg = core::AngleMath::toDegrees(
        std::atan2(zH, std::sqrt(xH * xH + yH * yH))
    );

    return EclipticToEquatorialCalculator::eclipticToEquatorial(
        eclipticLongitudeDeg,
        eclipticLatitudeDeg,
        AstronomicalTime::meanObliquityDeg(daysSinceJ2000)
    );
}

}  // namespace skygate::ephemeris
