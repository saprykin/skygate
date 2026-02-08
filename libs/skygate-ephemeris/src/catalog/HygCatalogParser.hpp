#pragma once

#include "skygate/ephemeris/StarCatalogFactory.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class HygCatalogParser final {
public:
    [[nodiscard]] std::vector<CelestialBody> parse(
        std::string_view csvData,
        const HygParseProgressCallback& progressCallback
    ) const;
};

}  // namespace skygate::ephemeris
