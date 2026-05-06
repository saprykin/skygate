#pragma once

#include "SkySelectionOverlayBuilder.hpp"

class SkySelectionMarkerBuilder final {
public:
    [[nodiscard]] SkySelectionMarker build(
        const SkySelectionOverlayInput& input
    ) const;
};
