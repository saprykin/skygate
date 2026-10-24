#include "CatalogHeaderPolicy.hpp"
#include "CsvRowTokenizer.hpp"
#include "StringUtilities.hpp"

#include <QString>
#include <QStringView>
#include <QVector>

#include <algorithm>

namespace skygate::ephemeris {

std::string_view CatalogHeaderPolicy::findHeaderLine(std::string_view& remaining) noexcept
{
    // A UTF-8 BOM marks the start of the payload, so it is stripped before the
    // header line is located. This is the only place that strips it, so the
    // returned line and the rest of `remaining` are always BOM-free.
    if (remaining.size() >= 3U && static_cast<unsigned char>(remaining[0]) == 0xEFU
        && static_cast<unsigned char>(remaining[1]) == 0xBBU && static_cast<unsigned char>(remaining[2]) == 0xBFU) {
        remaining.remove_prefix(3U);
    }

    while (!remaining.empty()) {
        std::string_view line = CsvRowTokenizer::takeLine(remaining);

        line = StringUtilities::trimAsciiWhitespace(line);

        if (line.empty()) {
            continue;
        }

        // Comments are allowed only before the header. A line is a comment
        // when '#' is its first non-whitespace character; a '#' inside a quoted
        // field is data and is never stripped here.
        if (line.front() == '#') {
            continue;
        }

        return line;
    }

    return {};
}

QHash<QString, qsizetype>
CatalogHeaderPolicy::decodeHeaderLine(const std::string_view headerLine, const QChar separator)
{
    const QString line = CsvRowTokenizer::decodeTrimmedLine(headerLine);
    const QVector<QStringView> columns = CsvRowTokenizer::splitColumns(QStringView{line}, separator);

    QHash<QString, qsizetype> headerIndex;
    headerIndex.reserve(columns.size());
    for (qsizetype index = 0; index < columns.size(); ++index) {
        QString header = CsvRowTokenizer::decodeTrimmedField(columns.at(index));
        header = header.toLower();
        if (!header.isEmpty()) {
            headerIndex.insert(header, index);
        }
    }

    return headerIndex;
}

bool CatalogHeaderPolicy::hasRequiredColumns(
    const QHash<QString, qsizetype>& header, const std::vector<QString>& requiredColumns
)
{
    return std::all_of(requiredColumns.begin(), requiredColumns.end(), [&header](const QString& name) {
        return header.contains(name.toLower());
    });
}

}  // namespace skygate::ephemeris
