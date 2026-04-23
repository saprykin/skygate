#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class ConstellationReferenceCalculator final {
public:
    [[nodiscard]] static std::optional<core::HorizontalCoordinate> labelCenter(
        const SkySnapshot& snapshot,
        std::span<const ConstellationLabelRef> labelRefs,
        std::string_view label
    );
};

}  // namespace skygate::ephemeris
