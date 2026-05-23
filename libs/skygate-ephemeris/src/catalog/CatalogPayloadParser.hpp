#pragma once

#include "catalog/CatalogLoadResult.hpp"
#include "catalog/CatalogParseRequest.hpp"
#include "catalog/CatalogSelectionOptions.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"

#include <memory>
#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadParser final {
public:
    [[nodiscard]] CatalogSourceType detectFormat(std::string_view payload) const noexcept;

    [[nodiscard]] CatalogLoadResult parseResult(const CatalogParseRequest& request) const;
    [[nodiscard]] CatalogLoadResult parseResult(
        std::string_view payload,
        const CatalogParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    ) const;

    [[nodiscard]] std::unique_ptr<IStarCatalog> parse(
        std::string_view payload,
        const CatalogParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    ) const;
};

}  // namespace skygate::ephemeris
