#pragma once

#include <cstdint>
#include <optional>

struct SkySceneCompositionResult final {
    bool changed = false;
    bool frameContentChanged = false;
    std::optional<std::uint32_t> trailTargetBodyIndex;
};
