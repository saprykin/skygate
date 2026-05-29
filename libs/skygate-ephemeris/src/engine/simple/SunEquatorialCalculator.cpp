#include "SunEquatorialCalculator.hpp"
#include "EclipticToEquatorialCalculator.hpp"
#include "math/AngleMath.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"

#include <cmath>

namespace skygate::ephemeris {

skygate::core::EquatorialCoordinate
SunEquatorialCalculator::compute(const skygate::core::UtcTimePoint& utcTime) const noexcept
{
    const double daysSinceJ2000 = EpochCodec::daysSinceJ2000(utcTime);
    const double meanLongitudeDeg = skygate::core::AngleMath::normalizeDegrees(280.460 + 0.9856474 * daysSinceJ2000);
    const double meanAnomalyDeg = skygate::core::AngleMath::normalizeDegrees(357.528 + 0.9856003 * daysSinceJ2000);
    const double meanAnomalyRad = skygate::core::AngleMath::toRadians(meanAnomalyDeg);

    const double eclipticLongitudeDeg = skygate::core::AngleMath::normalizeDegrees(
        meanLongitudeDeg + 1.915 * std::sin(meanAnomalyRad) + 0.020 * std::sin(2.0 * meanAnomalyRad)
    );

    return EclipticToEquatorialCalculator::compute(
        eclipticLongitudeDeg, 0.0, AstronomicalTime::meanObliquityDeg(daysSinceJ2000)
    );
}

}  // namespace skygate::ephemeris
