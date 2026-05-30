#include "ErfaAstrometry.hpp"

#include <cmath>

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
extern "C" {
#include <erfa.h>
}
#endif

namespace skygate::ephemeris::highprecision {
namespace {

[[nodiscard]] bool epochInScaleIsFinite(const AstronomicalEpoch& epoch, const TimeScale timeScale) noexcept
{
    return epoch.timeScale == timeScale && epoch.isFinite();
}

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
[[nodiscard]] skygate::core::Matrix3x3 matrixFromErfaRows(const double matrix[3][3]) noexcept
{
    return {
        {.x = matrix[0][0], .y = matrix[0][1], .z = matrix[0][2]},
        {.x = matrix[1][0], .y = matrix[1][1], .z = matrix[1][2]},
        {.x = matrix[2][0], .y = matrix[2][1], .z = matrix[2][2]},
    };
}
#endif

[[nodiscard]] skygate::core::Matrix3x3 earthRotationMatrixFromAngle(const double earthRotationAngle) noexcept
{
    const double sine = std::sin(earthRotationAngle);
    const double cosine = std::cos(earthRotationAngle);
    return {
        {.x = cosine, .y = sine, .z = 0.0},
        {.x = -sine, .y = cosine, .z = 0.0},
        {.x = 0.0, .y = 0.0, .z = 1.0},
    };
}

}  // namespace

std::optional<skygate::core::Matrix3x3>
ErfaAstrometry::celestialToIntermediateMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept
{
    if (!epochInScaleIsFinite(terrestrialTime, TimeScale::Tt)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    double matrix[3][3] = {};
    eraC2i06a(terrestrialTime.julianDatePart1, terrestrialTime.julianDatePart2, matrix);

    return matrixFromErfaRows(matrix);
#else
    return std::nullopt;
#endif
}

std::optional<skygate::core::Matrix3x3>
ErfaAstrometry::precessionNutationMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept
{
    if (!epochInScaleIsFinite(terrestrialTime, TimeScale::Tt)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    double matrix[3][3] = {};
    eraPnm06a(terrestrialTime.julianDatePart1, terrestrialTime.julianDatePart2, matrix);

    return matrixFromErfaRows(matrix);
#else
    return std::nullopt;
#endif
}

std::optional<skygate::core::Matrix3x3>
ErfaAstrometry::earthRotationMatrix00(const AstronomicalEpoch& universalTime1) noexcept
{
    if (!epochInScaleIsFinite(universalTime1, TimeScale::Ut1)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    return earthRotationMatrixFromAngle(eraEra00(universalTime1.julianDatePart1, universalTime1.julianDatePart2));
#else
    return std::nullopt;
#endif
}

std::optional<double> ErfaAstrometry::tioLocatorS00(const AstronomicalEpoch& terrestrialTime) noexcept
{
    if (!epochInScaleIsFinite(terrestrialTime, TimeScale::Tt)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    return eraSp00(terrestrialTime.julianDatePart1, terrestrialTime.julianDatePart2);
#else
    return std::nullopt;
#endif
}

std::optional<skygate::core::Matrix3x3> ErfaAstrometry::polarMotionMatrix00(
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
    return matrixFromErfaRows(matrix);
#else
    static_cast<void>(polarMotionXRadians);
    static_cast<void>(polarMotionYRadians);
    static_cast<void>(tioLocatorRadians);
    return std::nullopt;
#endif
}

std::optional<double> ErfaAstrometry::tdbMinusTtSeconds(
    const AstronomicalEpoch& terrestrialTime,
    const double ut1FractionOfDay,
    const double eastLongitudeRadians,
    const double distanceFromSpinAxisKm,
    const double distanceNorthOfEquatorialPlaneKm
) noexcept
{
    if (!epochInScaleIsFinite(terrestrialTime, TimeScale::Tt) || !std::isfinite(ut1FractionOfDay)
        || !std::isfinite(eastLongitudeRadians) || !std::isfinite(distanceFromSpinAxisKm)
        || !std::isfinite(distanceNorthOfEquatorialPlaneKm)) {
        return std::nullopt;
    }

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    return eraDtdb(
        terrestrialTime.julianDatePart1,
        terrestrialTime.julianDatePart2,
        ut1FractionOfDay,
        eastLongitudeRadians,
        distanceFromSpinAxisKm,
        distanceNorthOfEquatorialPlaneKm
    );
#else
    static_cast<void>(ut1FractionOfDay);
    static_cast<void>(eastLongitudeRadians);
    static_cast<void>(distanceFromSpinAxisKm);
    static_cast<void>(distanceNorthOfEquatorialPlaneKm);
    return std::nullopt;
#endif
}

}  // namespace skygate::ephemeris::highprecision
