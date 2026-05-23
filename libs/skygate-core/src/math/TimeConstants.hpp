#pragma once

namespace skygate::core {

class TimeConstants final {
public:
    static constexpr double kSecondsPerMinute = 60.0;
    static constexpr double kSecondsPerHour = 3'600.0;
    static constexpr double kSecondsPerDay = 86'400.0;
    static constexpr double kJulianDaysPerYear = 365.25;
    static constexpr double kSecondsPerJulianYear = kJulianDaysPerYear * kSecondsPerDay;
    static constexpr double kJulianDateJ2000 = 2'451'545.0;
    static constexpr double kTtMinusTaiSeconds = 32.184;
};

}  // namespace skygate::core
