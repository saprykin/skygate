#pragma once

#include "catalog/CatalogBodyParseResult.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <string_view>

namespace skygate::ephemeris {

class HygCatalogParser final {
public:
    [[nodiscard]] CatalogBodyParseResult parse(
        std::string_view csvData,
        const HygParseProgressCallback& progressCallback
    ) const;
};

}  // namespace skygate::ephemeris
