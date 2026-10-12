#pragma once

#include "SkySelectedObjectInspector.hpp"
#include "SkySelectionOverlayBuilder.hpp"
#include "SkySelectionOverlayInput.hpp"

class SkyObjectInspectorBuilder final {
public:
    [[nodiscard]] SkySelectedObjectInspector build(const SkySelectionOverlayInput& input) const;
};
