#include "DelimitedSchemaOptions.hpp"

#include <utility>

namespace skygate::ephemeris {

DelimitedCatalogReaderOptions DelimitedSchemaOptions::readerOptions(
    const CatalogSchemaDescriptor& schema,
    const CatalogLoadResult::ErrorCode invalidErrorCode,
    const std::size_t rowCountLimitFloor,
    const std::size_t minExpectedBytesPerDataRow
)
{
    return DelimitedCatalogReaderOptions{
        .separator = schema.delimiter,
        .requiredColumns = schema.requiredColumns,
        .invalidErrorCode = invalidErrorCode,
        .rowCountLimitFloor = rowCountLimitFloor,
        .minExpectedBytesPerDataRow = minExpectedBytesPerDataRow,
        .emptyInputDetail = schema.emptyInputDetail(),
        .missingColumnsDetail = schema.missingRequiredColumnsDetail(),
        .rowLimitDetail = schema.rowLimitDetail(),
    };
}

DelimitedCatalogParserOptions DelimitedSchemaOptions::parserOptions(
    const CatalogSchemaDescriptor& schema,
    const CatalogLoadResult::ErrorCode zeroResultCode,
    std::string zeroResultDetail
)
{
    return DelimitedCatalogParserOptions{
        .zeroResultCode = zeroResultCode,
        .zeroResultDetail = std::move(zeroResultDetail),
        .formatName = schema.diagnosticName,
    };
}

}  // namespace skygate::ephemeris
