#pragma once

#include "text/HighPrecisionTextParser.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisTableMetadata final {
public:
    EphemerisTableMetadata() = delete;

    [[nodiscard]] static const HighPrecisionTextParser& textParser() noexcept;

    [[nodiscard]] static bool isHeaderLine(std::string_view line) noexcept;

    [[nodiscard]] static EphemerisDateRange makeRange(
        std::string_view id,
        std::string_view displayName,
        const skygate::core::AstronomicalEpoch& start,
        const skygate::core::AstronomicalEpoch& end
    );

    // Applies the metadata keys shared by the table formats: 'version',
    // 'source', and 'expires'. Returns true when the line carries one of the
    // shared keys; a malformed 'expires' value reports diagnostic text.
    template <typename Info>
    [[nodiscard]] static bool applyCommonLine(
        Info& info,
        const std::string_view line,
        const std::string_view dataLabel,
        const std::size_t lineNumber,
        std::optional<std::string>& diagnosticText
    )
    {
        if (const std::optional<std::string> value = textParser().metadataValue(line, "version"); value.has_value()) {
            if (!value->empty()) {
                info.version = *value;
            }
            return true;
        }
        if (const std::optional<std::string> value = textParser().metadataValue(line, "source"); value.has_value()) {
            if (!value->empty()) {
                info.provenance = *value;
            }
            return true;
        }
        if (const std::optional<std::string> value = textParser().metadataValue(line, "expires"); value.has_value()) {
            const std::optional<skygate::core::CivilDateTime> expiresDate = textParser().parseUtcDate(*value);
            const std::optional<skygate::core::AstronomicalEpoch> expiresEpoch =
                expiresDate.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*expiresDate)
                                        : std::nullopt;
            if (!expiresEpoch.has_value()) {
                diagnosticText = std::string(dataLabel) + " contains malformed expiration metadata at line "
                                 + std::to_string(lineNumber) + ".";
                return true;
            }

            info.expiresAt = *expiresEpoch;
            return true;
        }

        return false;
    }
};

}  // namespace skygate::ephemeris
