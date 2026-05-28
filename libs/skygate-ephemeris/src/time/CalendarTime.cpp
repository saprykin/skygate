#include "CalendarTime.hpp"
#include "math/TimeConstants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace skygate::ephemeris {

using core::TimeConstants;

bool CalendarTime::isGregorianLeapYear(const int astronomicalYear) noexcept
{
    return astronomicalYear % 4 == 0 && (astronomicalYear % 100 != 0 || astronomicalYear % 400 == 0);
}

int CalendarTime::daysInGregorianMonth(const int astronomicalYear, const int month) noexcept
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

std::int64_t CalendarTime::daysFromCivilDate(const int astronomicalYear, const int month, const int day) noexcept
{
    int year = astronomicalYear;
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned monthPrime = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153U * monthPrime + 2U) / 5U + static_cast<unsigned>(day) - 1U;
    const unsigned dayOfEra = yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
    return static_cast<std::int64_t>(era) * 146'097LL + static_cast<std::int64_t>(dayOfEra) - 719'468LL;
}

CivilDateTime CalendarTime::civilDateFromDays(const std::int64_t daysSinceUnixEpoch) noexcept
{
    const std::int64_t z = daysSinceUnixEpoch + 719'468LL;
    const std::int64_t era = (z >= 0 ? z : z - 146'096LL) / 146'097LL;
    const unsigned dayOfEra = static_cast<unsigned>(z - era * 146'097LL);
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

std::optional<int> CalendarTime::astronomicalYearFromHistoricalYear(const int historicalYear) noexcept
{
    if (historicalYear == 0) {
        return std::nullopt;
    }

    if (historicalYear < 0) {
        return historicalYear + 1;
    }

    return historicalYear;
}

int CalendarTime::historicalYearFromAstronomicalYear(const int astronomicalYear) noexcept
{
    if (astronomicalYear <= 0) {
        return astronomicalYear - 1;
    }

    return astronomicalYear;
}

bool CalendarTime::isValidCivilDateTime(const CivilDateTime& dateTime) noexcept
{
    if (dateTime.month < 1 || dateTime.month > 12) {
        return false;
    }
    if (dateTime.day < 1 || dateTime.day > daysInGregorianMonth(dateTime.astronomicalYear, dateTime.month)) {
        return false;
    }
    if (dateTime.hour < 0 || dateTime.hour > 23 || dateTime.minute < 0 || dateTime.minute > 59) {
        return false;
    }
    if (dateTime.second < 0 || dateTime.second > 60) {
        return false;
    }
    if (dateTime.second == 60
        && (dateTime.timeScale != TimeScale::Utc || dateTime.hour != 23 || dateTime.minute != 59)) {
        return false;
    }

    return dateTime.nanosecond < TimeConstants::kNanosecondsPerSecond;
}

AstronomicalEpochResult CalendarTime::astronomicalEpochFromCivilDateTime(const CivilDateTime& dateTime) noexcept
{
    if (!isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }
    if (dateTime.second == 60) {
        return std::nullopt;
    }

    const std::int64_t daysSinceUnixEpoch = daysFromCivilDate(dateTime.astronomicalYear, dateTime.month, dateTime.day);
    const std::int64_t totalSeconds =
        static_cast<std::int64_t>(dateTime.hour) * static_cast<std::int64_t>(TimeConstants::kSecondsPerHour)
        + static_cast<std::int64_t>(dateTime.minute) * static_cast<std::int64_t>(TimeConstants::kSecondsPerMinute)
        + static_cast<std::int64_t>(dateTime.second);
    const std::int64_t nanosecondsSinceMidnight =
        totalSeconds * TimeConstants::kNanosecondsPerSecond + static_cast<std::int64_t>(dateTime.nanosecond);
    const double dayFraction =
        static_cast<double>(nanosecondsSinceMidnight) / static_cast<double>(TimeConstants::kNanosecondsPerDay);

    return AstronomicalEpoch{
        .julianDatePart1 = TimeConstants::kJulianDateUnixEpoch + static_cast<double>(daysSinceUnixEpoch),
        .julianDatePart2 = dayFraction,
        .timeScale = dateTime.timeScale,
    }
        .normalized();
}

CivilDateTimeResult CalendarTime::civilDateTimeFromAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept
{
    if (!std::isfinite(epoch.julianDatePart1) || !std::isfinite(epoch.julianDatePart2)) {
        return std::nullopt;
    }

    const AstronomicalEpoch normalizedEpoch = epoch.normalized();
    const double daysSinceUnixEpochDouble =
        (normalizedEpoch.julianDatePart1 - TimeConstants::kJulianDateUnixEpoch) + normalizedEpoch.julianDatePart2;
    if (!std::isfinite(daysSinceUnixEpochDouble)
        || daysSinceUnixEpochDouble < static_cast<double>(std::numeric_limits<std::int64_t>::min())
        || daysSinceUnixEpochDouble > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    auto wholeDays = static_cast<std::int64_t>(std::floor(daysSinceUnixEpochDouble));
    double dayFraction = daysSinceUnixEpochDouble - static_cast<double>(wholeDays);
    if (dayFraction < 0.0) {
        --wholeDays;
        dayFraction += 1.0;
    }

    auto nanosecondsSinceMidnight =
        static_cast<std::int64_t>(std::llround(dayFraction * static_cast<double>(TimeConstants::kNanosecondsPerDay)));
    if (nanosecondsSinceMidnight >= TimeConstants::kNanosecondsPerDay) {
        ++wholeDays;
        nanosecondsSinceMidnight = 0;
    }

    CivilDateTime dateTime = civilDateFromDays(wholeDays);
    dateTime.hour = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerHour);
    nanosecondsSinceMidnight %= TimeConstants::kNanosecondsPerHour;
    dateTime.minute = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerMinute);
    nanosecondsSinceMidnight %= TimeConstants::kNanosecondsPerMinute;
    dateTime.second = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerSecond);
    dateTime.nanosecond = static_cast<std::uint32_t>(nanosecondsSinceMidnight % TimeConstants::kNanosecondsPerSecond);
    dateTime.timeScale = normalizedEpoch.timeScale;
    return dateTime;
}

CivilDateTime CalendarTime::civilDateFromDecimalYear(const double decimalYear) noexcept
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

}  // namespace skygate::ephemeris
