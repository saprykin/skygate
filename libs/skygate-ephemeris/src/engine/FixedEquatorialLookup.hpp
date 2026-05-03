#pragma once

#include "skygate/core/Types.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

struct FixedEquatorialCoordinate final {
    const char* id;
    core::EquatorialCoordinate equatorial;
};

class FixedEquatorialLookup final {
public:
    [[nodiscard]] static std::optional<core::EquatorialCoordinate> find(
        std::span<const FixedEquatorialCoordinate> coordinates,
        std::string_view id
    ) noexcept;
};

}  // namespace skygate::ephemeris
