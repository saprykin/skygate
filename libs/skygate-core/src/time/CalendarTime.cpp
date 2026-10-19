#include "CalendarTime.hpp"
#include "EpochCodec.hpp"
#include "GregorianCalendar.hpp"
#include "math/TimeConstants.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace skygate::core {

using skygate::core::TimeConstants;

bool CalendarTime::isUtcLeapSecondLabel(const CivilDateTime& dateTime) noexcept
{
    return dateTime.timeScale == TimeScale::Utc && dateTime.hour == 23 && dateTime.minute == 59
           && dateTime.second == 60;
}

std::optional<CivilDateTime> CalendarTime::leapSecondPredecessor(const CivilDateTime& dateTime) noexcept
{
    if (!isUtcLeapSecondLabel(dateTime)) {
        return std::nullopt;
    }

    CivilDateTime precedingSecond = dateTime;
    precedingSecond.second = 59;
    return precedingSecond;
}

bool CalendarTime::isValidGregorianMonth(const int month) noexcept
{
    return month >= 1 && month <= 12;
}

bool CalendarTime::isValidCivilDateTime(const CivilDateTime& dateTime) noexcept
{
    if (!isValidGregorianMonth(dateTime.month)) {
        return false;
    }
    if (dateTime.day < 1
        || dateTime.day > GregorianCalendar::daysInGregorianMonth(dateTime.astronomicalYear, dateTime.month)) {
        return false;
    }
    if (dateTime.hour < 0 || dateTime.hour > 23 || dateTime.minute < 0 || dateTime.minute > 59) {
        return false;
    }
    if (dateTime.second < 0 || dateTime.second > 60) {
        return false;
    }
    if (dateTime.second == 60 && !isUtcLeapSecondLabel(dateTime)) {
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

    const std::int64_t daysSinceUnixEpoch =
        GregorianCalendar::daysFromCivilDate(dateTime.astronomicalYear, dateTime.month, dateTime.day);
    const std::int64_t totalSeconds =
        static_cast<std::int64_t>(dateTime.hour) * static_cast<std::int64_t>(TimeConstants::kSecondsPerHour)
        + static_cast<std::int64_t>(dateTime.minute) * static_cast<std::int64_t>(TimeConstants::kSecondsPerMinute)
        + static_cast<std::int64_t>(dateTime.second);
    const std::int64_t nanosecondsSinceMidnight =
        totalSeconds * TimeConstants::kNanosecondsPerSecond + static_cast<std::int64_t>(dateTime.nanosecond);
    const double dayFraction =
        static_cast<double>(nanosecondsSinceMidnight) / static_cast<double>(TimeConstants::kNanosecondsPerDay);

    return AstronomicalEpoch{
        .julianDatePart1 = EpochCodec::julianDateFromDaysSinceUnixEpoch(static_cast<double>(daysSinceUnixEpoch)),
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
        EpochCodec::daysSinceUnixEpoch(normalizedEpoch.julianDatePart1, normalizedEpoch.julianDatePart2);
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

    CivilDateTime dateTime = GregorianCalendar::civilDateFromDays(wholeDays);
    dateTime.hour = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerHour);
    nanosecondsSinceMidnight %= TimeConstants::kNanosecondsPerHour;
    dateTime.minute = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerMinute);
    nanosecondsSinceMidnight %= TimeConstants::kNanosecondsPerMinute;
    dateTime.second = static_cast<int>(nanosecondsSinceMidnight / TimeConstants::kNanosecondsPerSecond);
    dateTime.nanosecond = static_cast<std::uint32_t>(nanosecondsSinceMidnight % TimeConstants::kNanosecondsPerSecond);
    dateTime.timeScale = normalizedEpoch.timeScale;
    return dateTime;
}

CivilDateTimeResult
CalendarTime::civilDateFromUtcParts(const int astronomicalYear, const int month, const int day) noexcept
{
    const CivilDateTime dateTime{
        .astronomicalYear = astronomicalYear,
        .month = month,
        .day = day,
        .timeScale = TimeScale::Utc,
    };
    if (!isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }

    return dateTime;
}

}  // namespace skygate::core
