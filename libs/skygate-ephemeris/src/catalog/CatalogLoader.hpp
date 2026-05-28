#pragma once

#include "CatalogLoadResult.hpp"
#include "CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogLoader final {
public:
    [[nodiscard]] static CatalogLoadResult load(const CatalogSourceRequest& request);
    [[nodiscard]] static CatalogLoadResult load(
        CatalogSourceType type,
        std::string_view data = {},
        const CatalogParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    );
};

}  // namespace skygate::ephemeris
