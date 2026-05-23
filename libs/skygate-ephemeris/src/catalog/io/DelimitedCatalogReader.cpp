#include "catalog/io/DelimitedCatalogReader.hpp"
#include "catalog/io/CsvRowTokenizer.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <optional>

namespace skygate::ephemeris {
namespace {

std::optional<QHash<QString, qsizetype>> parseHeaderRow(
    const QVector<QStringView>& columns,
    const std::vector<QString>& requiredColumns,
    const std::string& missingColumnsDetail,
    CatalogBodyParseResult& result
)
{
    QHash<QString, qsizetype> headerIndex;
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

    const bool hasRequiredColumns =
        std::all_of(requiredColumns.begin(), requiredColumns.end(), [&headerIndex](const QString& name) {
            return headerIndex.contains(name.toLower());
        });
    if (!hasRequiredColumns) {
        result.errorCode = CatalogLoadResult::ErrorCode::MissingRequiredColumns;
        result.errorDetail = missingColumnsDetail;
        return std::nullopt;
    }

    return headerIndex;
}

}  // namespace

CatalogBodyParseResult DelimitedCatalogReader::read(
    const std::string_view payload,
    const DelimitedCatalogReaderOptions& options,
    const DelimitedCatalogRowHandler& rowHandler
)
{
    CatalogBodyParseResult result;
    if (payload.empty()) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = options.emptyInputDetail;
        return result;
    }

    const std::size_t expectedBytesPerDataRow = std::max<std::size_t>(options.minExpectedBytesPerDataRow, 1U);
    const std::size_t maxAllowedRowCount =
        std::max(options.rowCountLimitFloor, (payload.size() / expectedBytesPerDataRow) + 1U);

    std::size_t processedRowCount = 0;
    bool hasHeader = false;
    QHash<QString, qsizetype> headerIndex;

    for (const std::string_view rawLine : strings::splitView(payload, '\n')) {
        const QString line = QString::fromUtf8(rawLine.data(), static_cast<qsizetype>(rawLine.size())).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QVector<QStringView> columns = CsvRowTokenizer::splitColumns(QStringView{line}, options.separator);

        if (!hasHeader) {
            auto parsedHeader = parseHeaderRow(columns, options.requiredColumns, options.missingColumnsDetail, result);
            if (!parsedHeader) {
                return result;
            }
            headerIndex = std::move(*parsedHeader);
            hasHeader = true;
            continue;
        }

        if (++processedRowCount > maxAllowedRowCount) {
            result.errorCode = options.invalidErrorCode;
            result.errorDetail = options.rowLimitDetail;
            return result;
        }

        DelimitedCatalogRow row(std::move(columns), headerIndex);
        if (rowHandler && !rowHandler(row, result)) {
            return result;
        }
    }

    result.diagnostics.processedRowCount = processedRowCount;
    if (!hasHeader) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = options.emptyInputDetail;
    }

    return result;
}

}  // namespace skygate::ephemeris
