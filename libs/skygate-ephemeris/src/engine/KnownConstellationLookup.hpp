#pragma once

#include "skygate/core/Types.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class KnownConstellationLookup final {
public:
    [[nodiscard]] std::optional<core::EquatorialCoordinate> equatorialById(
        std::string_view constellationId
    ) const noexcept;
};

}  // namespace skygate::ephemeris
