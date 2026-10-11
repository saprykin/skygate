#pragma once

#include "StringUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class EphemerisTableMetadata final {
public:
    EphemerisTableMetadata() = delete;

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
        if (const std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "version");
            value.has_value()) {
            if (!value->empty()) {
                info.version = *value;
            }
            return true;
        }
        if (const std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "source");
            value.has_value()) {
            if (!value->empty()) {
                info.provenance = *value;
            }
            return true;
        }
        if (const std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "expires");
            value.has_value()) {
            const std::optional<skygate::core::CivilDateTime> expiresDate = TextParsingUtilities::parseUtcDate(*value);
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

    // Walks one table payload line by line, owning the line numbering, the
    // blank and comment handling, the shared header check, and the malformed
    // row diagnostic. The metadata handler consumes '#' lines it recognizes and
    // reports a malformed value through diagnosticText; the skip predicate drops
    // format-specific filler lines; the entry parser fills one table row. Lines
    // are numbered from one and the first malformed line ends the walk.
    template <typename MetadataLineHandler, typename SkipLinePredicate, typename EntryLineParser, typename Entry>
    [[nodiscard]] static std::optional<std::string> parseTableLines(
        const std::string_view content,
        MetadataLineHandler metadataLineHandler,
        SkipLinePredicate skipLinePredicate,
        EntryLineParser entryLineParser,
        std::vector<Entry>& entries,
        const std::string_view dataLabel
    )
    {
        std::string_view remaining = content;
        std::size_t lineNumber = 0U;
        while (!remaining.empty()) {
            ++lineNumber;
            const std::string_view rawLine = StringUtilities::takeLine(remaining);
            const std::string_view trimmedLine = StringUtilities::trimAsciiWhitespace(rawLine);
            if (trimmedLine.empty()) {
                continue;
            }
            if (trimmedLine.starts_with("#")) {
                std::optional<std::string> diagnosticText;
                if (metadataLineHandler(trimmedLine, lineNumber, diagnosticText) && diagnosticText.has_value()) {
                    return diagnosticText;
                }
                continue;
            }
            if (isHeaderLine(trimmedLine) || skipLinePredicate(rawLine, trimmedLine, entries.empty())) {
                continue;
            }

            Entry entry;
            if (!entryLineParser(rawLine, trimmedLine, entry)) {
                return std::string(dataLabel) + " contains a malformed row at line " + std::to_string(lineNumber) + ".";
            }
            entries.push_back(entry);
        }

        return std::nullopt;
    }
};

}  // namespace skygate::ephemeris
