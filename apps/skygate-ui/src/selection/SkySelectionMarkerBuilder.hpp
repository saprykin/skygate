#pragma once

#include "SkySelectionMarker.hpp"
#include "SkySelectionOverlayBuilder.hpp"
#include "SkySelectionOverlayInput.hpp"

class SkySelectionMarkerBuilder final {
public:
    [[nodiscard]] SkySelectionMarker build(const SkySelectionOverlayInput& input) const;
};
