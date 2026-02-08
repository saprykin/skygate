#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class CatalogRowParser final {
public:
    [[nodiscard]] std::vector<CelestialBody> parse(std::string_view catalogRows) const;
};

}  // namespace skygate::ephemeris
