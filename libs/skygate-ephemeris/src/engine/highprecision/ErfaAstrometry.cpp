#include "engine/highprecision/ErfaAstrometry.hpp"

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

}  // namespace skygate::ephemeris::highprecision
