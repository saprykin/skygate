#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class StellariumConstellationParser final {
public:
    struct ParseResult {
        std::vector<ConstellationLineRef> lineRefs;
        std::vector<ConstellationAnchorGroup> anchorGroups;
        std::size_t constellationCount = 0;
    };

    [[nodiscard]] ParseResult parse(std::string_view payload) const;
};

}  // namespace skygate::ephemeris
