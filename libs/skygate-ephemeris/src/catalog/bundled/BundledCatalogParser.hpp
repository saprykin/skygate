#pragma once

#include "catalog/CatalogBodyParseResult.hpp"

namespace skygate::ephemeris {

class BundledCatalogParser final {
public:
    [[nodiscard]] CatalogBodyParseResult parse() const;
};

}  // namespace skygate::ephemeris
