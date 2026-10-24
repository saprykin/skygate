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

    // True when the payload's first non-whitespace byte starts a JSON object or
    // array. A whitespace-only payload does not look like JSON.
    [[nodiscard]] static bool looksLikeJsonPayload(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
