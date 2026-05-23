#pragma once

#include "catalog/model/CatalogBodyParseResult.hpp"
#include "skygate/ephemeris/CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

class HygCatalogParser final {
public:
    [[nodiscard]] CatalogBodyParseResult
    parse(std::string_view csvData, const HygParseProgressCallback& progressCallback) const;
};

}  // namespace skygate::ephemeris
