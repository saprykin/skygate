#include "engine/highprecision/ErfaAstrometry.hpp"

#include <cmath>

extern "C" {
#include <erfa.h>
}

namespace skygate::ephemeris::highprecision {

std::optional<JulianDateParts> calendarDateToJulianDate(const int year, const int month, const int day) noexcept
{
    double day1 = 0.0;
    double day2 = 0.0;
    const int status = eraCal2jd(year, month, day, &day1, &day2);
    if (status != 0) {
        return std::nullopt;
    }

    return JulianDateParts{
        .day1 = day1,
        .day2 = day2,
    };
}

std::optional<double> tdbMinusTtSeconds(
    const JulianDateParts terrestrialTime,
    const double ut1FractionOfDay,
    const double eastLongitudeRadians,
    const double distanceFromSpinAxisKm,
    const double distanceNorthOfEquatorialPlaneKm
) noexcept
{
    if (!std::isfinite(terrestrialTime.day1) || !std::isfinite(terrestrialTime.day2) || !std::isfinite(ut1FractionOfDay)
        || !std::isfinite(eastLongitudeRadians) || !std::isfinite(distanceFromSpinAxisKm)
        || !std::isfinite(distanceNorthOfEquatorialPlaneKm)) {
        return std::nullopt;
    }

    return eraDtdb(
        terrestrialTime.day1,
        terrestrialTime.day2,
        ut1FractionOfDay,
        eastLongitudeRadians,
        distanceFromSpinAxisKm,
        distanceNorthOfEquatorialPlaneKm
    );
}

}  // namespace skygate::ephemeris::highprecision
