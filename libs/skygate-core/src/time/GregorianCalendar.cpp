#include "GregorianCalendar.hpp"
#include "math/TimeConstants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace skygate::core {
namespace {

// Day count between the Unix epoch and the start of a Gregorian era, and the
// number of days in one 400-year era.
constexpr std::int64_t kUnixEpochCivilDayOffset = 719'468LL;
constexpr std::int64_t kDaysPerGregorianEra = 146'097LL;

}  // namespace

using skygate::core::TimeConstants;

bool GregorianCalendar::isGregorianLeapYear(const int astronomicalYear) noexcept
{
    return astronomicalYear % 4 == 0 && (astronomicalYear % 100 != 0 || astronomicalYear % 400 == 0);
}

int GregorianCalendar::daysInGregorianMonth(const int astronomicalYear, const int month) noexcept
{
    switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return isGregorianLeapYear(astronomicalYear) ? 29 : 28;
    default:
        return 0;
    }
}

std::int64_t GregorianCalendar::daysFromCivilDate(const int astronomicalYear, const int month, const int day) noexcept
{
    int year = astronomicalYear;
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned monthPrime = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153U * monthPrime + 2U) / 5U + static_cast<unsigned>(day) - 1U;
    const unsigned dayOfEra = yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
    return static_cast<std::int64_t>(era) * kDaysPerGregorianEra + static_cast<std::int64_t>(dayOfEra)
           - kUnixEpochCivilDayOffset;
}

CivilDateTime GregorianCalendar::civilDateFromDays(const std::int64_t daysSinceUnixEpoch) noexcept
{
    const std::int64_t z = daysSinceUnixEpoch + kUnixEpochCivilDayOffset;
    const std::int64_t era = (z >= 0 ? z : z - (kDaysPerGregorianEra - 1)) / kDaysPerGregorianEra;
    const unsigned dayOfEra = static_cast<unsigned>(z - era * kDaysPerGregorianEra);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1'460U + dayOfEra / 36'524U - dayOfEra / 146'096U) / 365U;
    const int year = static_cast<int>(yearOfEra) + static_cast<int>(era) * 400;
    const unsigned dayOfYear = dayOfEra - (365U * yearOfEra + yearOfEra / 4U - yearOfEra / 100U);
    const unsigned monthPrime = (5U * dayOfYear + 2U) / 153U;
    const unsigned day = dayOfYear - (153U * monthPrime + 2U) / 5U + 1U;
    const int month = static_cast<int>(monthPrime) + (monthPrime < 10U ? 3 : -9);

    return CivilDateTime{
        .astronomicalYear = year + (month <= 2 ? 1 : 0),
        .month = month,
        .day = static_cast<int>(day),
    };
}

std::optional<int> GregorianCalendar::astronomicalYearFromHistoricalYear(const int historicalYear) noexcept
{
    if (historicalYear == 0) {
        return std::nullopt;
    }

    if (historicalYear < 0) {
        return historicalYear + 1;
    }

    return historicalYear;
}

int GregorianCalendar::historicalYearFromAstronomicalYear(const int astronomicalYear) noexcept
{
    if (astronomicalYear <= 0) {
        return astronomicalYear - 1;
    }

    return astronomicalYear;
}

CivilDateTime GregorianCalendar::civilDateFromDecimalYear(const double decimalYear) noexcept
{
    const int year = static_cast<int>(std::floor(decimalYear));
    const double fraction = decimalYear - static_cast<double>(year);
    const int daysInYear = isGregorianLeapYear(year) ? static_cast<int>(TimeConstants::kDaysPerLeapYear)
                                                     : static_cast<int>(TimeConstants::kDaysPerCommonYear);
    int dayOffset = static_cast<int>(std::floor(fraction * static_cast<double>(daysInYear) + 0.5));
    dayOffset = std::clamp(dayOffset, 0, daysInYear - 1);

    CivilDateTime date = civilDateFromDays(daysFromCivilDate(year, 1, 1) + dayOffset);
    date.timeScale = TimeScale::Utc;
    return date;
}

}  // namespace skygate::core
