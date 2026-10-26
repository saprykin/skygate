#pragma once

#include "CelestialBodyState.hpp"
#include "EphemerisSnapshot.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineQueries final {
public:
    [[nodiscard]] static std::optional<CelestialBodyState>
    findBodyStateById(const EphemerisSnapshot& snapshot, std::string_view bodyId);

    [[nodiscard]] static std::optional<CelestialBodyState>
    findBodyStateByIndex(const EphemerisSnapshot& snapshot, std::uint32_t bodyIndex);
};

}  // namespace skygate::ephemeris
