#pragma once

#include <optional>

namespace skygate::ephemeris::highprecision {

struct JulianDateParts {
    double day1 = 0.0;
    double day2 = 0.0;
};

[[nodiscard]] std::optional<JulianDateParts> calendarDateToJulianDate(int year, int month, int day) noexcept;

}  // namespace skygate::ephemeris::highprecision
