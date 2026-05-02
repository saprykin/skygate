#pragma once

#include "SkyRenderBuilders.hpp"
#include "skygate/core/math/ScreenGeometry.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

class SkyHitTargetIndex final {
public:
    void rebuild(
        const SkyRenderFrame& frame,
        const skygate::ephemeris::SkySnapshot& snapshot
    );
    void clear();
    [[nodiscard]] std::optional<std::uint32_t> bodyIndexAt(
        double x,
        double y,
        double viewportWidth,
        double viewportHeight,
        const skygate::ephemeris::SkySnapshot& snapshot
    ) const;

private:
    std::vector<skygate::core::CircleHitTarget> m_targets;
    skygate::core::CircleHitIndex m_hitIndex {24.0};
};
