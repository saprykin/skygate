#include "CelestialReferenceCalculator.hpp"
#include "math/TimeConstants.hpp"
#include "engine/simple/EclipticToEquatorialCalculator.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"

#include <cmath>

namespace skygate::ephemeris {

using core::TimeConstants;

core::HorizontalCoordinate CelestialReferenceCalculator::eclipticPoint(
    const double eclipticLongitudeDeg, const core::GeoLocation& observer, const core::UtcTimePoint& utcTime
) noexcept
{
    const double obliquityDeg = AstronomicalTime::meanObliquityDeg(EpochCodec::daysSinceJ2000(utcTime));
    return EquatorialToHorizontalCalculator::compute(
        EclipticToEquatorialCalculator::compute(eclipticLongitudeDeg, 0.0, obliquityDeg), observer, utcTime
    );
}

core::HorizontalCoordinate CelestialReferenceCalculator::equatorialPoint(
    const double rightAscensionHours,
    const double declinationDeg,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    return EquatorialToHorizontalCalculator::compute(
        core::EquatorialCoordinate{.rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg},
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

    const double rightAscensionHours =
        TimeConstants::kHoursPerDay * static_cast<double>(sampleIndex) / static_cast<double>(sampleCount);
    return equatorialPoint(rightAscensionHours, declinationDeg, observer, utcTime);
}

double CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(const core::GeoLocation& observer) noexcept
{
    return (observer.latitudeDeg >= 0.0 ? 1.0 : -1.0) * (90.0 - std::abs(observer.latitudeDeg));
}

}  // namespace skygate::ephemeris
