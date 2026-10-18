#pragma once

#include "EphemerisTableDataStatus.hpp"
#include "StringUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/EphemerisDateRange.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class EphemerisTableMetadata final {
public:
    /// Canonical prefix of a metadata line inside a table payload;
    /// isMetadataLine() also accepts the marker with other separators.
    inline static constexpr std::string_view kMetadataLinePrefix = "#@ ";

    EphemerisTableMetadata() = delete;

    [[nodiscard]] static bool isHeaderLine(std::string_view line) noexcept;

    [[nodiscard]] static bool isMetadataLine(std::string_view line) noexcept;

    [[nodiscard]] static EphemerisDateRange makeRange(
        std::string_view id,
        std::string_view displayName,
        const skygate::core::AstronomicalEpoch& start,
        const skygate::core::AstronomicalEpoch& end
    );

    [[nodiscard]] static std::string
    malformedMetadata(std::string_view dataLabel, std::string_view subject, std::size_t lineNumber);

    /// Parses one endpoint date of a metadata range and stores it in `range`
    /// together with the supplied range identity. A malformed date reports a
    /// subject-specific diagnostic and leaves the range untouched.
    [[nodiscard]] static std::optional<std::string> assignRangeEndpoint(
        EphemerisDateRange& range,
        skygate::core::AstronomicalEpoch EphemerisDateRange::* endpoint,
        std::string_view value,
        std::string_view rangeId,
        std::string_view rangeDisplayName,
        std::string_view dataLabel,
        std::string_view subject,
        std::size_t lineNumber,
        bool& hasEndpoint
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
                diagnosticText = malformedMetadata(dataLabel, "expiration", lineNumber);
                return true;
            }

            info.expiresAt = *expiresEpoch;
            return true;
        }

        return false;
    }

    // Walks one table payload line by line, owning the line numbering, the
    // blank and comment handling, the shared header check, and the malformed
    // row diagnostic. The metadata handler sees the metadata lines only and
    // reports a malformed value through diagnosticText; the skip predicate
    // drops format-specific filler lines; the entry parser fills one table
    // row. Lines are numbered from one and the first malformed line ends the
    // walk.
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
                if (isMetadataLine(trimmedLine) && metadataLineHandler(trimmedLine, lineNumber, diagnosticText)
                    && diagnosticText.has_value()) {
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

    // Applies the checks and the success tail that the table parsers share:
    // an empty payload, rows out of UTC order, optionally duplicate UTC dates,
    // and the validity range, status, and diagnostic of a parsed table.
    // `rowLabel` names the rows in the ordering and duplicate diagnostics.
    // `endValidityRangeAtExpiration` reports the published expiration as the
    // range end instead of the last row.
    template <typename Info, typename Entry>
    [[nodiscard]] static std::optional<std::string> finalizeParsedTable(
        Info& info,
        const std::vector<Entry>& entries,
        const std::string_view dataLabel,
        const std::string_view rowLabel,
        const std::string_view validityRangeId,
        const bool rejectDuplicateDates,
        const bool endValidityRangeAtExpiration
    )
    {
        if (entries.empty()) {
            return std::string(dataLabel) + " contains no rows.";
        }

        const auto epochOf = [](const Entry& entry) { return entry.effectiveUtcEpoch.sortKey(); };
        if (!std::ranges::is_sorted(entries, {}, epochOf)) {
            return std::string(rowLabel) + " rows are not sorted by UTC date.";
        }
        if (rejectDuplicateDates && std::ranges::adjacent_find(entries, {}, epochOf) != entries.end()) {
            return std::string(rowLabel) + " rows contain duplicate UTC dates.";
        }

        const bool useExpiration = endValidityRangeAtExpiration && info.expiresAt.has_value();
        const skygate::core::AstronomicalEpoch& rangeEnd =
            useExpiration ? *info.expiresAt : entries.back().effectiveUtcEpoch;
        info.validityRange = makeRange(validityRangeId, dataLabel, entries.front().effectiveUtcEpoch, rangeEnd);
        info.status = EphemerisTableDataStatus::Available;
        info.diagnosticText = std::string(dataLabel) + " loaded.";
        return std::nullopt;
    }
};

}  // namespace skygate::ephemeris
