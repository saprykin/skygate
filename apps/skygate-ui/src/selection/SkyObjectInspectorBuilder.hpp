#pragma once

#include "SkySelectionOverlayInput.hpp"
#include "scene/SkySelectedObjectInspector.hpp"

class SkyObjectInspectorBuilder final {
public:
    [[nodiscard]] SkySelectedObjectInspector build(const SkySelectionOverlayInput& input) const;
};
