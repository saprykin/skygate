#include "DelimitedCatalogReader.hpp"
#include "CatalogHeaderPolicy.hpp"
#include "CsvRowTokenizer.hpp"
#include "StringUtilities.hpp"

#include <algorithm>

namespace skygate::ephemeris {

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
    std::string_view remaining = payload;
    const std::string_view headerLine = CatalogHeaderPolicy::findHeaderLine(remaining);
    if (headerLine.empty()) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = options.emptyInputDetail;
        return result;
    }

    const QHash<QString, qsizetype> headerIndex = CatalogHeaderPolicy::decodeHeaderLine(headerLine, options.separator);
    if (!CatalogHeaderPolicy::hasRequiredColumns(headerIndex, options.requiredColumns)) {
        result.errorCode = CatalogLoadResult::ErrorCode::MissingRequiredColumns;
        result.errorDetail = options.missingColumnsDetail;
        return result;
    }

    for (const std::string_view rawLine : StringUtilities::splitView(remaining, '\n')) {
        const QString line = QString::fromUtf8(rawLine.data(), static_cast<qsizetype>(rawLine.size())).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (++processedRowCount > maxAllowedRowCount) {
            result.errorCode = options.invalidErrorCode;
            result.errorDetail = options.rowLimitDetail;
            return result;
        }

        QVector<QStringView> columns = CsvRowTokenizer::splitColumns(QStringView{line}, options.separator);
        DelimitedCatalogRow row(std::move(columns), headerIndex);
        if (rowHandler && !rowHandler(row, result)) {
            return result;
        }
    }

    result.diagnostics.processedRowCount = processedRowCount;
    return result;
}

}  // namespace skygate::ephemeris
