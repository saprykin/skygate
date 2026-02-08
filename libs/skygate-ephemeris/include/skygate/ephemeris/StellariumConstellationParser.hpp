#pragma once

#include "skygate/ephemeris/ConstellationData.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class StellariumConstellationParser final {
public:
    struct ParseResult {
        std::vector<ConstellationLineRef> lineRefs;
        std::vector<ConstellationLabelRef> labelRefs;
        std::size_t constellationCount = 0;
        bool isJsonPayload = false;
    };

    [[nodiscard]] ParseResult parse(std::string_view payload) const;
};

}  // namespace skygate::ephemeris
