#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class ConstellationDataCodec final {
public:
    [[nodiscard]] static std::string serializeLineRows(std::span<const ConstellationLineRef> lineRefs);
    [[nodiscard]] static std::vector<ConstellationLineRef> parseLineRows(std::string_view rows);

    [[nodiscard]] static std::string serializeAnchorGroupRows(std::span<const ConstellationAnchorGroup> anchorGroups);
    [[nodiscard]] static std::vector<ConstellationAnchorGroup> parseAnchorGroupRows(std::string_view rows);
};

}  // namespace skygate::ephemeris
