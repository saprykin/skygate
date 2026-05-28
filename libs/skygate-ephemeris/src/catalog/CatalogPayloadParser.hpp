#pragma once

#include "CatalogLoadResult.hpp"
#include "CatalogParseRequest.hpp"
#include "CatalogSelectionOptions.hpp"
#include "CatalogSourceType.hpp"
#include "IStarCatalog.hpp"

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
