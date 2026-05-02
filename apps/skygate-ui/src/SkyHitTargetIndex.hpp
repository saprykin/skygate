#pragma once

#include "SkyRenderBuilders.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
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
    struct Target final {
        std::uint32_t bodyIndex = 0;
        double x = 0.0;
        double y = 0.0;
        double radiusPx = 0.0;
    };

    std::vector<Target> m_targets;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_targetIndicesByCell;
};
