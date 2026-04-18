#pragma once

#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <string_view>

namespace skygate::ephemeris {

struct CatalogParseRequest {
    std::string_view payload;
    HygParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
};

class CatalogPayloadParser final {
public:
    [[nodiscard]] CatalogPayloadFormat detectFormat(std::string_view payload) const noexcept;

    [[nodiscard]] CatalogLoadResult parseResult(
        const CatalogParseRequest& request
    ) const;
    [[nodiscard]] CatalogLoadResult parseResult(
        std::string_view payload,
        const HygParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    ) const;

    [[nodiscard]] std::unique_ptr<IStarCatalog> parse(
        std::string_view payload,
        const HygParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    ) const;
};

}  // namespace skygate::ephemeris
