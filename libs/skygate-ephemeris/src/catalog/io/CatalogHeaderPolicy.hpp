#pragma once

#include <QChar>
#include <QHash>
#include <QString>

#include <QtGlobal>

#include <string_view>
#include <vector>

namespace skygate::ephemeris {

// Shared header preparation and tokenization policy for catalog readers and
// the payload format detector. Both sides use the same rules for the UTF-8
// BOM, leading blank lines, leading full-line comments, quoting, and case
// normalization so detection and parsing cannot disagree about a header.
class CatalogHeaderPolicy final {
public:
    CatalogHeaderPolicy() = delete;

    // Advances `remaining` past a leading UTF-8 BOM, blank lines, and full-line
    // comments. A line is a comment only when '#' is its first non-whitespace
    // character; a '#' inside a quoted field is never treated as a comment.
    // Returns the first header line without its line terminator and leaves
    // `remaining` positioned after that line. Returns an empty view when no
    // header line exists.
    [[nodiscard]] static std::string_view findHeaderLine(std::string_view& remaining) noexcept;

    // Decodes a header line into a lowercased column-name -> original-column
    // index map. Quoted fields are decoded, empty names are omitted, and a BOM
    // on the first field is removed.
    [[nodiscard]] static QHash<QString, qsizetype> decodeHeaderLine(std::string_view headerLine, QChar separator);

    // Returns true when the decoded header contains every required column,
    // compared case-insensitively.
    [[nodiscard]] static bool
    hasRequiredColumns(const QHash<QString, qsizetype>& header, const std::vector<QString>& requiredColumns);
};

}  // namespace skygate::ephemeris
