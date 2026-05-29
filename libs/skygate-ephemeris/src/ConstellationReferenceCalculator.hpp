#pragma once

#include "EphemerisSnapshot.hpp"
#include "HorizontalCoordinate.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class ConstellationReferenceCalculator final {
public:
    [[nodiscard]] static std::optional<skygate::core::HorizontalCoordinate> anchorCentroid(
        const EphemerisSnapshot& snapshot,
        std::span<const ConstellationAnchorGroup> anchorGroups,
        std::string_view labelName
    );
};

}  // namespace skygate::ephemeris
