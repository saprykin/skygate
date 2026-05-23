#pragma once

#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/ICatalogParser.hpp"

#include <string_view>

namespace skygate::ephemeris {

class BundledCatalogParser final : public ICatalogParser {
public:
    [[nodiscard]] CatalogBodyParseResult
    parse(std::string_view data, const CatalogParseProgressCallback& progressCallback) const override;
};

}  // namespace skygate::ephemeris
