#pragma once

#include "DelimitedCatalogParserOptions.hpp"
#include "DelimitedCatalogReaderOptions.hpp"
#include "catalog/CatalogSchemaDescriptor.hpp"

#include <cstddef>
#include <string>

namespace skygate::ephemeris {

// Reader and runner options of one registered delimited schema.
//
// A delimited parser states only what is specific to its source: the row-limit
// floor and expected bytes per row of the source, the error code it reports, and
// the detail of an empty result. Everything the schema already describes
// (delimiter, required columns, diagnostic name, and the schema-worded
// diagnostics) is resolved here, so the descriptor and the reader can never
// describe different column sets or messages.
class DelimitedSchemaOptions final {
public:
    DelimitedSchemaOptions() = delete;

    [[nodiscard]] static DelimitedCatalogReaderOptions readerOptions(
        const CatalogSchemaDescriptor& schema,
        CatalogLoadResult::ErrorCode invalidErrorCode,
        std::size_t rowCountLimitFloor,
        std::size_t minExpectedBytesPerDataRow
    );

    [[nodiscard]] static DelimitedCatalogParserOptions parserOptions(
        const CatalogSchemaDescriptor& schema, CatalogLoadResult::ErrorCode zeroResultCode, std::string zeroResultDetail
    );
};

}  // namespace skygate::ephemeris
