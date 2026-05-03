#include "engine/FixedEquatorialLookup.hpp"

namespace skygate::ephemeris {

std::optional<core::EquatorialCoordinate> FixedEquatorialLookup::find(
    const std::span<const FixedEquatorialCoordinate> coordinates,
    const std::string_view id
) noexcept
{
    for (const FixedEquatorialCoordinate& coordinate : coordinates) {
        if (id == coordinate.id) {
            return coordinate.equatorial;
        }
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
