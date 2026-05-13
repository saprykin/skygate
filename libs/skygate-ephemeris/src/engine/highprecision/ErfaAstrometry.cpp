#include "engine/highprecision/ErfaAstrometry.hpp"

#include <cmath>

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
extern "C" {
#include <erfa.h>
}
#endif

namespace skygate::ephemeris::highprecision {

std::optional<JulianDateParts> calendarDateToJulianDate(const int year, const int month, const int day) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
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
#else
    static_cast<void>(year);
    static_cast<void>(month);
    static_cast<void>(day);
    return std::nullopt;
#endif
}

std::optional<Matrix3x3> celestialToIntermediateMatrix06A(const JulianDateParts terrestrialTime) noexcept
{
    if (!std::isfinite(terrestrialTime.day1) || !std::isfinite(terrestrialTime.day2)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    double matrix[3][3] = {};
    eraC2i06a(terrestrialTime.day1, terrestrialTime.day2, matrix);

    return Matrix3x3{
        std::array<double, 3>{matrix[0][0], matrix[0][1], matrix[0][2]},
        std::array<double, 3>{matrix[1][0], matrix[1][1], matrix[1][2]},
        std::array<double, 3>{matrix[2][0], matrix[2][1], matrix[2][2]},
    };
#else
    return std::nullopt;
#endif
}

std::optional<double> earthRotationAngle00(const JulianDateParts universalTime1) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    if (!std::isfinite(universalTime1.day1) || !std::isfinite(universalTime1.day2)) {
        return std::nullopt;
    }

    return eraEra00(universalTime1.day1, universalTime1.day2);
#else
    static_cast<void>(universalTime1);
    return std::nullopt;
#endif
}

std::optional<double> tioLocatorS00(const JulianDateParts terrestrialTime) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    if (!std::isfinite(terrestrialTime.day1) || !std::isfinite(terrestrialTime.day2)) {
        return std::nullopt;
    }

    return eraSp00(terrestrialTime.day1, terrestrialTime.day2);
#else
    static_cast<void>(terrestrialTime);
    return std::nullopt;
#endif
}

std::optional<Matrix3x3> polarMotionMatrix00(
    const double polarMotionXRadians, const double polarMotionYRadians, const double tioLocatorRadians
) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    if (!std::isfinite(polarMotionXRadians) || !std::isfinite(polarMotionYRadians)
        || !std::isfinite(tioLocatorRadians)) {
        return std::nullopt;
    }

    double matrix[3][3] = {};
    eraPom00(polarMotionXRadians, polarMotionYRadians, tioLocatorRadians, matrix);
    return Matrix3x3{
        std::array<double, 3>{matrix[0][0], matrix[0][1], matrix[0][2]},
        std::array<double, 3>{matrix[1][0], matrix[1][1], matrix[1][2]},
        std::array<double, 3>{matrix[2][0], matrix[2][1], matrix[2][2]},
    };
#else
    static_cast<void>(polarMotionXRadians);
    static_cast<void>(polarMotionYRadians);
    static_cast<void>(tioLocatorRadians);
    return std::nullopt;
#endif
}

std::optional<double> tdbMinusTtSeconds(
    const JulianDateParts terrestrialTime,
    const double ut1FractionOfDay,
    const double eastLongitudeRadians,
    const double distanceFromSpinAxisKm,
    const double distanceNorthOfEquatorialPlaneKm
) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
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
#else
    static_cast<void>(terrestrialTime);
    static_cast<void>(ut1FractionOfDay);
    static_cast<void>(eastLongitudeRadians);
    static_cast<void>(distanceFromSpinAxisKm);
    static_cast<void>(distanceNorthOfEquatorialPlaneKm);
    return std::nullopt;
#endif
}

}  // namespace skygate::ephemeris::highprecision
