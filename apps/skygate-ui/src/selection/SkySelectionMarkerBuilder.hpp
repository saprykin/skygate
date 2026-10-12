#pragma once

#include "SkySelectionOverlayBuilder.hpp"
#include "SkySelectionOverlayInput.hpp"
#include "scene/SkySelectionMarker.hpp"

class SkySelectionMarkerBuilder final {
public:
    [[nodiscard]] SkySelectionMarker build(const SkySelectionOverlayInput& input) const;
};
