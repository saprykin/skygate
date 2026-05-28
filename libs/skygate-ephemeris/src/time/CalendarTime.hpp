#pragma once

#include "AstronomicalEpoch.hpp"
#include "CivilDateTime.hpp"

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {

class CalendarTime final {
public:
    [[nodiscard]] static bool isValidCivilDateTime(const CivilDateTime& dateTime) noexcept;

    [[nodiscard]] static AstronomicalEpochResult
    astronomicalEpochFromCivilDateTime(const CivilDateTime& dateTime) noexcept;

    [[nodiscard]] static CivilDateTimeResult
    civilDateTimeFromAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept;

    [[nodiscard]] static std::optional<int> astronomicalYearFromHistoricalYear(int historicalYear) noexcept;
    [[nodiscard]] static int historicalYearFromAstronomicalYear(int astronomicalYear) noexcept;

    [[nodiscard]] static bool isGregorianLeapYear(int astronomicalYear) noexcept;
    [[nodiscard]] static int daysInGregorianMonth(int astronomicalYear, int month) noexcept;
    [[nodiscard]] static std::int64_t daysFromCivilDate(int astronomicalYear, int month, int day) noexcept;
    [[nodiscard]] static CivilDateTime civilDateFromDays(std::int64_t daysSinceUnixEpoch) noexcept;
    [[nodiscard]] static CivilDateTime civilDateFromDecimalYear(double decimalYear) noexcept;
};

}  // namespace skygate::ephemeris
