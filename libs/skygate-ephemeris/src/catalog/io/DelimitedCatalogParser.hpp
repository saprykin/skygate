#pragma once

#include "catalog/io/DelimitedCatalogReaderOptions.hpp"
#include "catalog/io/DelimitedCatalogRow.hpp"
#include "catalog/io/DelimitedCatalogParserOptions.hpp"
#include "catalog/io/RowParseOutcome.hpp"
#include "catalog/CatalogSourceRequest.hpp"

#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class DelimitedCatalogParser final {
public:
    using RowMapper = std::function<RowParseOutcome(const DelimitedCatalogRow& row, std::size_t rowNumber)>;

    [[nodiscard]] static CatalogBodyParseResult
    run(std::string_view payload,
        const DelimitedCatalogReaderOptions& readerOptions,
        const DelimitedCatalogParserOptions& runnerOptions,
        std::span<const std::string_view> invalidCategoryLabels,
        const RowMapper& rowMapper,
        const CatalogParseProgressCallback& progressCallback);
};

}  // namespace skygate::ephemeris
