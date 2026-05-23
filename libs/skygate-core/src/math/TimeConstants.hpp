#pragma once

#include <cstdint>

namespace skygate::core {

class TimeConstants final {
public:
    static constexpr double kHoursPerDay = 24.0;
    static constexpr double kDaysPerCommonYear = 365.0;
    static constexpr double kDaysPerLeapYear = kDaysPerCommonYear + 1.0;
    static constexpr double kSecondsPerMinute = 60.0;
    static constexpr double kSecondsPerHour = 3'600.0;
    static constexpr double kSecondsPerDay = 86'400.0;
    static constexpr double kMinutesPerHour = 60.0;
    static constexpr double kMinutesPerDay = kHoursPerDay * kMinutesPerHour;
    static constexpr double kJulianDaysPerYear = 365.25;
    static constexpr double kSecondsPerJulianYear = kJulianDaysPerYear * kSecondsPerDay;
    static constexpr double kJulianDateUnixEpoch = 2'440'587.5;
    static constexpr double kJulianDateJ2000 = 2'451'545.0;
    static constexpr double kTtMinusTaiSeconds = 32.184;

    static constexpr double kMicrosecondsPerSecond = 1'000'000.0;
    static constexpr double kMicrosecondsPerDay = kSecondsPerDay * kMicrosecondsPerSecond;

    static constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000LL;
    static constexpr std::int64_t kNanosecondsPerMinute =
        static_cast<std::int64_t>(kSecondsPerMinute) * kNanosecondsPerSecond;
    static constexpr std::int64_t kNanosecondsPerHour =
        static_cast<std::int64_t>(kSecondsPerHour) * kNanosecondsPerSecond;
    static constexpr std::int64_t kNanosecondsPerDay =
        static_cast<std::int64_t>(kSecondsPerDay) * kNanosecondsPerSecond;
};

}  // namespace skygate::core
