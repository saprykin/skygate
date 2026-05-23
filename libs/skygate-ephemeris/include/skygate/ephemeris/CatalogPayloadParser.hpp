#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/CatalogSelectionOptions.hpp"
#include "skygate/ephemeris/CatalogSourceRequest.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
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

    [[nodiscard]] CatalogLoadResult parseResult(const CatalogParseRequest& request) const;
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
