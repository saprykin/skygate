#pragma once

#include "catalog/model/CatalogBodyParseResult.hpp"
#include "catalog/CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

class OpenNgcCatalogParser final {
public:
    [[nodiscard]] CatalogBodyParseResult
    parse(std::string_view csvData, const HygParseProgressCallback& progressCallback) const;
};

}  // namespace skygate::ephemeris
