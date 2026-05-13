#pragma once

#include <array>
#include <optional>

namespace skygate::ephemeris::highprecision {

using Matrix3x3 = std::array<std::array<double, 3>, 3>;

struct JulianDateParts {
    double day1 = 0.0;
    double day2 = 0.0;
};

[[nodiscard]] std::optional<JulianDateParts> calendarDateToJulianDate(int year, int month, int day) noexcept;
[[nodiscard]] std::optional<Matrix3x3> celestialToIntermediateMatrix06A(JulianDateParts terrestrialTime) noexcept;
[[nodiscard]] std::optional<double> earthRotationAngle00(JulianDateParts universalTime1) noexcept;
[[nodiscard]] std::optional<double> tioLocatorS00(JulianDateParts terrestrialTime) noexcept;
[[nodiscard]] std::optional<Matrix3x3>
polarMotionMatrix00(double polarMotionXRadians, double polarMotionYRadians, double tioLocatorRadians) noexcept;
[[nodiscard]] std::optional<double> tdbMinusTtSeconds(
    JulianDateParts terrestrialTime,
    double ut1FractionOfDay,
    double eastLongitudeRadians = 0.0,
    double distanceFromSpinAxisKm = 0.0,
    double distanceNorthOfEquatorialPlaneKm = 0.0
) noexcept;

}  // namespace skygate::ephemeris::highprecision
