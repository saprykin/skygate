#pragma once

#include "AstronomicalEpoch.hpp"
#include "CivilDateTime.hpp"

#include <optional>

namespace skygate::core {

class CalendarTime final {
public:
    [[nodiscard]] static bool isValidGregorianMonth(int month) noexcept;
    [[nodiscard]] static bool isValidCivilDateTime(const CivilDateTime& dateTime) noexcept;
    [[nodiscard]] static bool isUtcLeapSecondLabel(const CivilDateTime& dateTime) noexcept;
    [[nodiscard]] static std::optional<CivilDateTime> leapSecondPredecessor(const CivilDateTime& dateTime) noexcept;

    [[nodiscard]] static AstronomicalEpochResult
    astronomicalEpochFromCivilDateTime(const CivilDateTime& dateTime) noexcept;

    [[nodiscard]] static CivilDateTimeResult
    civilDateTimeFromAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept;

    [[nodiscard]] static CivilDateTimeResult civilDateFromUtcParts(int astronomicalYear, int month, int day) noexcept;
};

}  // namespace skygate::core
