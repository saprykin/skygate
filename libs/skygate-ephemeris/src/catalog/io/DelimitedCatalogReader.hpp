#pragma once

#include "catalog/io/DelimitedCatalogReaderOptions.hpp"
#include "catalog/io/DelimitedCatalogRow.hpp"

#include <string_view>

namespace skygate::ephemeris {

class DelimitedCatalogReader final {
public:
    [[nodiscard]] static CatalogBodyParseResult read(
        std::string_view payload,
        const DelimitedCatalogReaderOptions& options,
        const DelimitedCatalogRowHandler& rowHandler
    );
};

}  // namespace skygate::ephemeris
