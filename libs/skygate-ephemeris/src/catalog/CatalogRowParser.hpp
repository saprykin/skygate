#pragma once

#include "catalog/CatalogBodyParseResult.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogRowParser final {
public:
    [[nodiscard]] CatalogBodyParseResult parse(std::string_view catalogRows) const;
};

}  // namespace skygate::ephemeris
