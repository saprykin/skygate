#pragma once

#include "skygate/core/Types.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class StarEquatorialCalculator final {
public:
    [[nodiscard]] std::optional<core::EquatorialCoordinate> compute(
        std::string_view bodyId
    ) const noexcept;
};

}  // namespace skygate::ephemeris
