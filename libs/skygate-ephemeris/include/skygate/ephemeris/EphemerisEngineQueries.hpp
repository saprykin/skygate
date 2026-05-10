#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineQueries final {
public:
    [[nodiscard]] static std::optional<CelestialBodyState> findBodyStateById(
        const SkySnapshot& snapshot,
        std::string_view bodyId
    );

    [[nodiscard]] static std::optional<CelestialBodyState> findBodyStateByIndex(
        const SkySnapshot& snapshot,
        std::uint32_t bodyIndex
    );

    [[nodiscard]] static std::optional<CelestialBodyState> computeBodyStateById(
        const IEphemerisEngine& engine,
        const core::SkyContext& context,
        std::string_view bodyId
    );

    [[nodiscard]] static std::optional<CelestialBodyState> computeBodyStateByIndex(
        const IEphemerisEngine& engine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex
    );
};

}  // namespace skygate::ephemeris
