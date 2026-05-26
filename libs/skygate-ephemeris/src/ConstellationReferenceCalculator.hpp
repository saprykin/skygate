#pragma once

#include "HorizontalCoordinate.hpp"
#include "Types.hpp"

#include "catalog/constellation/ConstellationData.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class ConstellationReferenceCalculator final {
public:
    [[nodiscard]] static std::optional<core::HorizontalCoordinate> anchorCentroid(
        const SkySnapshot& snapshot, std::span<const ConstellationAnchorGroup> anchorGroups, std::string_view labelName
    );
};

}  // namespace skygate::ephemeris
