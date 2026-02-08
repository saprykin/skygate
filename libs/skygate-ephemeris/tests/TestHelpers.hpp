#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <cmath>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::tests {

[[nodiscard]] inline bool isNear(
    const double value,
    const double expected,
    const double tolerance
) noexcept
{
    return std::abs(value - expected) <= tolerance;
}

[[nodiscard]] inline const CelestialBody* findBodyById(
    const std::vector<CelestialBody>& bodies,
    const std::string_view id
)
{
    for (const CelestialBody& body : bodies) {
        if (body.id == id) {
            return &body;
        }
    }
    return nullptr;
}

[[nodiscard]] inline const CelestialBodyState* findStateById(
    const SkySnapshot& snapshot,
    const std::string_view id
)
{
    for (const CelestialBodyState& state : snapshot.states) {
        if (state.body.id == id) {
            return &state;
        }
    }
    return nullptr;
}

}  // namespace skygate::ephemeris::tests
