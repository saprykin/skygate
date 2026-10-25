#pragma once

#include "EphemerisTableDataStatus.hpp"
#include "EphemerisTableMetadata.hpp"
#include "EphemerisTextDataAsset.hpp"
#include "time/EphemerisDateRange.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

/// Shared skeleton of the ephemeris table parsers. It owns the payload walk
/// with its format hooks, the metadata range endpoints that the table infos
/// keep in optional fields, and the failure report; each parser keeps its own
/// metadata keys, its own row decoding, and its own result type.
class EphemerisTableParsing final {
public:
    /// Table-level settings of one payload walk.
    struct TableParseSpec {
        std::string_view dataLabel;
        std::string_view rowLabel;
        std::string_view validityRangeId;
        bool rejectDuplicateDates = false;
        bool endValidityRangeAtExpiration = false;
    };

    EphemerisTableParsing() = delete;

    /// Walks one table payload with the handlers of a parser, lets the parser
    /// validate the parsed table, and finalizes it. The returned diagnostic
    /// text of the first failing step ends the walk.
    template <
        typename Info,
        typename Entry,
        typename MetadataLineHandler,
        typename SkipLinePredicate,
        typename EntryLineParser,
        typename FormatValidation>
    [[nodiscard]] static std::optional<std::string> parsePayload(
        const EphemerisTextDataAsset& asset,
        Info& info,
        std::vector<Entry>& entries,
        MetadataLineHandler metadataLineHandler,
        SkipLinePredicate skipLinePredicate,
        EntryLineParser entryLineParser,
        const TableParseSpec& spec,
        FormatValidation formatValidation
    )
    {
        info.version = asset.version;
        info.provenance = asset.provenance;

        if (std::optional<std::string> malformedLine = EphemerisTableMetadata::parseTableLines(
                asset.content, metadataLineHandler, skipLinePredicate, entryLineParser, entries, spec.dataLabel
            );
            malformedLine.has_value()) {
            return malformedLine;
        }

        if (std::optional<std::string> diagnosticText = formatValidation(info); diagnosticText.has_value()) {
            return diagnosticText;
        }

        return EphemerisTableMetadata::finalizeParsedTable(
            info,
            entries,
            spec.dataLabel,
            spec.rowLabel,
            spec.validityRangeId,
            spec.rejectDuplicateDates,
            spec.endValidityRangeAtExpiration
        );
    }

    /// Records a failed parse in the info of one table.
    template <typename Info>
    static void markFailed(Info& info, std::string diagnosticText)
    {
        info.status = EphemerisTableDataStatus::Malformed;
        info.diagnosticText = std::move(diagnosticText);
    }

    /// Mutates the optional field, creating it on demand.
    template <typename Field, typename Mutate>
    static void updateOptionalField(std::optional<Field>& field, Mutate mutate)
    {
        Field updated = field.value_or(Field{});
        mutate(updated);
        field = std::move(updated);
    }

    /// Skip predicate of a payload that has no format-specific filler lines.
    [[nodiscard]] static bool
    skipNoLines(const std::string_view rawLine, const std::string_view trimmedLine, const bool entriesEmpty) noexcept
    {
        static_cast<void>(rawLine);
        static_cast<void>(trimmedLine);
        static_cast<void>(entriesEmpty);
        return false;
    }
};

}  // namespace skygate::ephemeris
