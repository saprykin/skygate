#pragma once

#include "skygate/ephemeris/StarCatalogFactory.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class HygGzipCatalogParser final {
public:
    [[nodiscard]] std::vector<CelestialBody> parse(
        std::string_view gzipData,
        const HygParseProgressCallback& progressCallback
    ) const;
};

}  // namespace skygate::ephemeris
