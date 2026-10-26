#pragma once

#include <QString>
#include <QStringView>
#include <QVector>

#include <string_view>

namespace skygate::ephemeris {

// The one delimited-text policy of the catalog readers: it advances over lines
// with a single terminator rule, splits a line into columns around the
// separator while keeping quoted separators inside their field, and decodes a
// field or a whole line from UTF-8 text. Header probing and row reading share
// these rules so detection and parsing cannot disagree about a payload.
class CsvRowTokenizer final {
public:
    CsvRowTokenizer() = delete;

    // Advances `remaining` past one line and returns that line without its
    // terminator. Both LF and CRLF end a line; a payload without a final
    // newline yields its last line once. The line policy lives in
    // StringUtilities::takeLine, which this forwards to.
    [[nodiscard]] static std::string_view takeLine(std::string_view& remaining) noexcept;

    [[nodiscard]] static QVector<QStringView> splitColumns(QStringView line);
    [[nodiscard]] static QVector<QStringView> splitColumns(QStringView line, QChar separator);
    [[nodiscard]] static QString decodeField(QStringView field);
    [[nodiscard]] static QString decodeTrimmedField(QStringView field);

    // Decodes one payload line to UTF-8 text with surrounding whitespace
    // removed.
    [[nodiscard]] static QString decodeTrimmedLine(std::string_view line);
};

}  // namespace skygate::ephemeris
