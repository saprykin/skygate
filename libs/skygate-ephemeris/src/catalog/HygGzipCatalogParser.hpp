#pragma once

#include "catalog/CatalogBodyParseResult.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <string_view>

namespace skygate::ephemeris {

class HygGzipCatalogParser final {
public:
    [[nodiscard]] CatalogBodyParseResult parse(
        std::string_view gzipData,
        const HygParseProgressCallback& progressCallback
    ) const;
};

}  // namespace skygate::ephemeris
