#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual SkySnapshot compute(const core::SkyContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        std::string_view bodyId
    ) const;

    [[nodiscard]] virtual std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        std::uint32_t bodyIndex
    ) const;
};

}  // namespace skygate::ephemeris
