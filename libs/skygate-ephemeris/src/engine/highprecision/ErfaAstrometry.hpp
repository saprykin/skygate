#pragma once

#include <optional>

namespace skygate::ephemeris::highprecision {

struct JulianDateParts {
    double day1 = 0.0;
    double day2 = 0.0;
};

[[nodiscard]] std::optional<JulianDateParts> calendarDateToJulianDate(int year, int month, int day) noexcept;
[[nodiscard]] std::optional<double> tdbMinusTtSeconds(
    JulianDateParts terrestrialTime,
    double ut1FractionOfDay,
    double eastLongitudeRadians = 0.0,
    double distanceFromSpinAxisKm = 0.0,
    double distanceNorthOfEquatorialPlaneKm = 0.0
) noexcept;

}  // namespace skygate::ephemeris::highprecision
