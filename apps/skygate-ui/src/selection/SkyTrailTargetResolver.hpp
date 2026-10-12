#pragma once

#include "SkySelectionOverlayBuilder.hpp"
#include "SkySelectionOverlayInput.hpp"

#include <cstdint>
#include <optional>

class SkyTrailTargetResolver final {
public:
    [[nodiscard]] QString activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] std::optional<std::uint32_t> activeTrailTargetBodyIndex(const SkySelectionOverlayInput& input) const;
};
