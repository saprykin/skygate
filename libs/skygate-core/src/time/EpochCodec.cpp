#include "EpochCodec.hpp"
#include "CalendarTime.hpp"
#include "UtcTimeCodec.hpp"
#include "math/TimeConstants.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace skygate::core {

using skygate::core::TimeConstants;

AstronomicalEpoch EpochCodec::epochFromUtcTime(const UtcTimePoint& utcTime) noexcept
{
    const double julianDay = julianDayFromUtc(utcTime);
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

AstronomicalEpoch EpochCodec::epochFromUtcTimeExact(const UtcTimePoint& utcTime) noexcept
{
    const std::int64_t epochSeconds = UtcTimeCodec::toEpochSecondsFloor(utcTime);
    const std::int64_t remainingMicroseconds = UtcTimeCodec::toEpochMicros(utcTime) - epochSeconds * 1'000'000;
    const std::int64_t secondsPerDay = static_cast<std::int64_t>(TimeConstants::kSecondsPerDay);
    const std::int64_t daysSinceUnix =
        std::chrono::floor<std::chrono::days>(std::chrono::seconds(epochSeconds)).count();
    const std::int64_t secondsOfDay = epochSeconds - daysSinceUnix * secondsPerDay;

    CivilDateTime dateTime = CalendarTime::civilDateFromDays(daysSinceUnix);
    dateTime.hour = static_cast<int>(secondsOfDay / static_cast<std::int64_t>(TimeConstants::kSecondsPerHour));
    dateTime.minute = static_cast<int>(
        secondsOfDay % static_cast<std::int64_t>(TimeConstants::kSecondsPerHour)
        / static_cast<std::int64_t>(TimeConstants::kSecondsPerMinute)
    );
    dateTime.second = static_cast<int>(secondsOfDay % static_cast<std::int64_t>(TimeConstants::kSecondsPerMinute));
    dateTime.nanosecond = static_cast<std::uint32_t>(remainingMicroseconds * 1'000);
    dateTime.timeScale = TimeScale::Utc;

    const AstronomicalEpochResult epoch = CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
    assert(epoch.has_value());
    return epoch.value_or(AstronomicalEpoch{.timeScale = TimeScale::Utc});
}

UtcTimePoint EpochCodec::utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = epoch.normalized();
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double epochMicros =
        std::round((julianDay - TimeConstants::kJulianDateUnixEpoch) * TimeConstants::kMicrosecondsPerDay);
    return UtcTimeCodec::fromEpochMicros(static_cast<std::int64_t>(epochMicros));
}

AstronomicalEpoch EpochCodec::epochFromNtpTimestamp(const std::uint64_t ntpTimestamp) noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = TimeConstants::kJulianDateNtpEpoch,
        .julianDatePart2 = static_cast<double>(ntpTimestamp) / TimeConstants::kSecondsPerDay,
        .timeScale = TimeScale::Utc,
    }
        .normalized();
}

AstronomicalEpoch EpochCodec::j2000Epoch(const TimeScale scale) noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = TimeConstants::kJulianDateJ2000,
        .julianDatePart2 = 0.0,
        .timeScale = scale,
    };
}

double EpochCodec::julianDayFromUtc(const UtcTimePoint& utcTime) noexcept
{
    return julianDateFromDaysSinceUnixEpoch(
        UtcTimeCodec::secondsSinceEpochDouble(utcTime) / TimeConstants::kSecondsPerDay
    );
}

double EpochCodec::julianDayFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.sortKey();
}

double EpochCodec::julianDateFromDaysSinceUnixEpoch(const double daysSinceUnix) noexcept
{
    return TimeConstants::kJulianDateUnixEpoch + daysSinceUnix;
}

double EpochCodec::daysSinceUnixEpoch(const double julianPart1, const double julianPart2) noexcept
{
    return (julianPart1 - TimeConstants::kJulianDateUnixEpoch) + julianPart2;
}

double EpochCodec::daysSinceJ2000(const UtcTimePoint& utcTime) noexcept
{
    return julianDayFromUtc(utcTime) - TimeConstants::kJulianDateJ2000;
}

double EpochCodec::daysSinceJ2000(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.sortKey() - TimeConstants::kJulianDateJ2000;
}

}  // namespace skygate::core
