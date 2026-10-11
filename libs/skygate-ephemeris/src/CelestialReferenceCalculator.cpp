#include "CelestialReferenceCalculator.hpp"
#include "FixedBodyAltitudeRange.hpp"
#include "math/TimeConstants.hpp"
#include "reference/EclipticToEquatorialCalculator.hpp"
#include "reference/EquatorialToHorizontalCalculator.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"

namespace skygate::ephemeris {

using skygate::core::TimeConstants;

skygate::core::HorizontalCoordinate CelestialReferenceCalculator::eclipticPoint(
    const double eclipticLongitudeDeg,
    const skygate::core::GeoLocation& observer,
    const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    const double obliquityDeg =
        skygate::core::AstronomicalTime::meanObliquityDeg(skygate::core::EpochCodec::daysSinceJ2000(utcTime));
    return EquatorialToHorizontalCalculator::compute(
        EclipticToEquatorialCalculator::compute(eclipticLongitudeDeg, 0.0, obliquityDeg), observer, utcTime
    );
}

skygate::core::HorizontalCoordinate CelestialReferenceCalculator::equatorialPoint(
    const double rightAscensionHours,
    const double declinationDeg,
    const skygate::core::GeoLocation& observer,
    const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    return EquatorialToHorizontalCalculator::compute(
        skygate::core::EquatorialCoordinate{
            .rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg
        },
        observer,
        utcTime
    );
}

skygate::core::HorizontalCoordinate CelestialReferenceCalculator::declinationCirclePoint(
    const int sampleIndex,
    const int sampleCount,
    const double declinationDeg,
    const skygate::core::GeoLocation& observer,
    const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    if (sampleCount <= 0) {
        return equatorialPoint(0.0, declinationDeg, observer, utcTime);
    }

    const double rightAscensionHours =
        TimeConstants::kHoursPerDay * static_cast<double>(sampleIndex) / static_cast<double>(sampleCount);
    return equatorialPoint(rightAscensionHours, declinationDeg, observer, utcTime);
}

double
CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(const skygate::core::GeoLocation& observer) noexcept
{
    // The boundary declination equals the signed altitude a celestial-equator
    // body reaches at culmination.
    const double hemisphereSign = observer.latitudeDeg >= 0.0 ? 1.0 : -1.0;
    return hemisphereSign * FixedBodyAltitudeRange::maxAltitudeDeg(observer.latitudeDeg, 0.0);
}

}  // namespace skygate::ephemeris
