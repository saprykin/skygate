#pragma once

#include "CivilDateTime.hpp"

#include <cstdint>

namespace skygate::core {

class GregorianCalendar final {
public:
    [[nodiscard]] static bool isGregorianLeapYear(int astronomicalYear) noexcept;
    [[nodiscard]] static int daysInGregorianMonth(int astronomicalYear, int month) noexcept;
    [[nodiscard]] static std::int64_t daysFromCivilDate(int astronomicalYear, int month, int day) noexcept;
    [[nodiscard]] static CivilDateTime civilDateFromDays(std::int64_t daysSinceUnixEpoch) noexcept;
    [[nodiscard]] static CivilDateTime civilDateFromDecimalYear(double decimalYear) noexcept;

    [[nodiscard]] static int historicalYearFromAstronomicalYear(int astronomicalYear) noexcept;
};

}  // namespace skygate::core
