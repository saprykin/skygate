#include "CatalogHeaderPolicy.hpp"
#include "CsvRowTokenizer.hpp"

#include <QString>
#include <QStringView>
#include <QVector>

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] bool isAsciiWhitespace(const char character) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

}  // namespace

std::string_view CatalogHeaderPolicy::findHeaderLine(std::string_view& remaining) noexcept
{
    // A UTF-8 BOM marks the start of the payload, so it is stripped before the
    // header line is located.
    if (remaining.size() >= 3U && static_cast<unsigned char>(remaining[0]) == 0xEFU
        && static_cast<unsigned char>(remaining[1]) == 0xBBU && static_cast<unsigned char>(remaining[2]) == 0xBFU) {
        remaining.remove_prefix(3U);
    }

    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        std::string_view line = newline == std::string_view::npos ? remaining : remaining.substr(0U, newline);
        remaining = newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1U);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        while (!line.empty() && isAsciiWhitespace(line.front())) {
            line.remove_prefix(1U);
        }
        while (!line.empty() && isAsciiWhitespace(line.back())) {
            line.remove_suffix(1U);
        }

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
    const QString line = QString::fromUtf8(headerLine.data(), static_cast<qsizetype>(headerLine.size())).trimmed();
    const QVector<QStringView> columns = CsvRowTokenizer::splitColumns(QStringView{line}, separator);

    QHash<QString, qsizetype> headerIndex;
    headerIndex.reserve(columns.size());
    for (qsizetype index = 0; index < columns.size(); ++index) {
        QString header = CsvRowTokenizer::decodeField(columns.at(index)).trimmed();
        if (index == 0 && header.startsWith(QChar(0xfeff))) {
            header.remove(0, 1);
        }
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
