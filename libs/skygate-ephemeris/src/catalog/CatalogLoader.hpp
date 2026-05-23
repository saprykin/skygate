#pragma once

#include "catalog/CatalogLoadResult.hpp"
#include "catalog/CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogLoader final {
public:
    [[nodiscard]] static CatalogLoadResult load(const CatalogSourceRequest& request);
    [[nodiscard]] static CatalogLoadResult load(
        CatalogSourceType type,
        std::string_view data = {},
        const HygParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    );
};

}  // namespace skygate::ephemeris
