#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"

#include "engine/CoordinateTransform.hpp"
#include "engine/JulianDateTime.hpp"

#include <cmath>

namespace skygate::ephemeris {

core::HorizontalCoordinate CelestialReferenceCalculator::eclipticPoint(
    const double eclipticLongitudeDeg,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    const double obliquityDeg = JulianDateTime::meanObliquityDeg(
        JulianDateTime::daysSinceJ2000(utcTime)
    );
    return CoordinateTransform::equatorialToHorizontal(
        CoordinateTransform::eclipticToEquatorial(eclipticLongitudeDeg, 0.0, obliquityDeg),
        observer,
        utcTime
    );
}

core::HorizontalCoordinate CelestialReferenceCalculator::equatorialPoint(
    const double rightAscensionHours,
    const double declinationDeg,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    return CoordinateTransform::equatorialToHorizontal(
        core::EquatorialCoordinate {
            .rightAscensionHours = rightAscensionHours,
            .declinationDeg = declinationDeg
        },
        observer,
        utcTime
    );
}

core::HorizontalCoordinate CelestialReferenceCalculator::declinationCirclePoint(
    const int sampleIndex,
    const int sampleCount,
    const double declinationDeg,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    if (sampleCount <= 0) {
        return equatorialPoint(0.0, declinationDeg, observer, utcTime);
    }

    const double rightAscensionHours = 24.0 * static_cast<double>(sampleIndex)
        / static_cast<double>(sampleCount);
    return equatorialPoint(rightAscensionHours, declinationDeg, observer, utcTime);
}

double CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
    const core::GeoLocation& observer
) noexcept
{
    return (observer.latitudeDeg >= 0.0 ? 1.0 : -1.0)
        * (90.0 - std::abs(observer.latitudeDeg));
}

}  // namespace skygate::ephemeris
