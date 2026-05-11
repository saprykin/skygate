#pragma once

#include "SkySelectionOverlayBuilder.hpp"

class SkyObjectInspectorBuilder final {
public:
    [[nodiscard]] SkySelectedObjectInspector build(
        const SkySelectionOverlayInput& input
    ) const;
};
