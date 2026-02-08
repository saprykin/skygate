#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <vector>

namespace skygate::ephemeris {

class BundledCatalogParser final {
public:
    [[nodiscard]] std::vector<CelestialBody> parse() const;
};

}  // namespace skygate::ephemeris
