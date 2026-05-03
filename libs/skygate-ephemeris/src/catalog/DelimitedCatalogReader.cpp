#include "catalog/DelimitedCatalogReader.hpp"

#include "catalog/CsvRowTokenizer.hpp"

#include <algorithm>
#include <utility>

namespace skygate::ephemeris {

DelimitedCatalogReader::Row::Row(
    QVector<QStringView> columns,
    const QHash<QString, qsizetype>& headerIndex
) noexcept
    : m_columns(std::move(columns))
    , m_headerIndex(headerIndex)
{
}

QString DelimitedCatalogReader::Row::decodeColumn(const QString& name) const
{
    return decodeColumn(columnIndex(name));
}

QString DelimitedCatalogReader::Row::decodeColumn(const qsizetype columnIndex) const
{
    if (columnIndex < 0 || columnIndex >= m_columns.size()) {
        return {};
    }

    return CsvRowTokenizer::decodeField(m_columns.at(columnIndex)).trimmed();
}

qsizetype DelimitedCatalogReader::Row::columnIndex(const QString& name) const
{
    const auto it = m_headerIndex.constFind(name.toLower());
    if (it == m_headerIndex.cend()) {
        return -1;
    }
    return *it;
}

CatalogBodyParseResult DelimitedCatalogReader::read(
    const std::string_view payload,
    const DelimitedCatalogReaderOptions& options,
    const RowHandler& rowHandler
)
{
    CatalogBodyParseResult result;
    if (payload.empty()) {
        result.errorCode = CatalogLoadErrorCode::EmptyInput;
        result.errorDetail = options.emptyInputDetail;
        return result;
    }

    const std::size_t plausibleRowBytes = std::max<std::size_t>(options.minPlausibleRowBytes, 1U);
    const std::size_t maxAllowedRowCount = std::max(
        options.minRowCountLimit,
        (payload.size() / plausibleRowBytes) + 1U
    );

    std::size_t cursor = 0;
    std::size_t processedRowCount = 0;
    bool hasHeader = false;
    QHash<QString, qsizetype> headerIndex;

    while (cursor < payload.size()) {
        const std::size_t newline = payload.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? payload.size() : newline;
        const std::string_view rawLine = payload.substr(cursor, lineEnd - cursor);
        const QString line = QString::fromUtf8(
            rawLine.data(),
            static_cast<qsizetype>(rawLine.size())
        ).trimmed();

        if (!line.isEmpty()) {
            QVector<QStringView> columns = CsvRowTokenizer::splitColumns(QStringView {line}, options.separator);
            if (!hasHeader) {
                for (qsizetype i = 0; i < columns.size(); ++i) {
                    QString header = CsvRowTokenizer::decodeField(columns.at(i)).trimmed();
                    if (i == 0 && header.startsWith(QChar(0xfeff))) {
                        header.remove(0, 1);
                    }
                    header = header.toLower();
                    if (!header.isEmpty()) {
                        headerIndex.insert(header, i);
                    }
                }

                const bool hasRequiredColumns = std::all_of(
                    options.requiredColumns.begin(),
                    options.requiredColumns.end(),
                    [&headerIndex](const QString& columnName) {
                        return headerIndex.contains(columnName.toLower());
                    }
                );
                if (!hasRequiredColumns) {
                    result.errorCode = CatalogLoadErrorCode::MissingRequiredColumns;
                    result.errorDetail = options.missingColumnsDetail;
                    return result;
                }
                hasHeader = true;
            } else {
                ++processedRowCount;
                if (processedRowCount > maxAllowedRowCount) {
                    result.errorCode = options.invalidErrorCode;
                    result.errorDetail = options.rowLimitDetail;
                    return result;
                }

                Row row(std::move(columns), headerIndex);
                if (rowHandler && !rowHandler(row, result)) {
                    return result;
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1U;
    }

    result.diagnostics.processedRowCount = processedRowCount;
    if (!hasHeader) {
        result.errorCode = CatalogLoadErrorCode::EmptyInput;
        result.errorDetail = options.emptyInputDetail;
    }

    return result;
}

}  // namespace skygate::ephemeris
