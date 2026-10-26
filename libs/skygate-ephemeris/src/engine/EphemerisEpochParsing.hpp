#pragma once

#include "TextParsingUtilities.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

/// Decodes the UTC date values of the ephemeris payloads and metadata into the
/// epochs that the table providers store. Every table format reads its dates
/// through this class so the accepted date syntax and the calendar conversion
/// stay in one place.
class EphemerisEpochParsing final {
public:
    EphemerisEpochParsing() = delete;

    /// UTC epoch of a civil date result, when the date is a valid calendar
    /// date.
    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch>
    epochFromCivilDate(const skygate::core::CivilDateTimeResult& date) noexcept
    {
        return date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    }

    /// UTC epoch of one UTC date column value, for example "2026-04-01".
    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch>
    epochFromDateColumn(const std::string_view text) noexcept
    {
        return epochFromCivilDate(TextParsingUtilities::parseUtcDate(text));
    }
};

}  // namespace skygate::ephemeris
