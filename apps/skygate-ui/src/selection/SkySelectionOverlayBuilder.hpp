#pragma once

#include "SkySelectionOverlayInput.hpp"
#include "scene/SkySelectedObjectInspector.hpp"
#include "scene/SkySelectionMarker.hpp"

#include <QString>

#include <cstdint>
#include <optional>

class SkySelectionOverlayBuilder final {
public:
    [[nodiscard]] SkySelectionMarker buildSelectionMarkerData(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] SkySelectedObjectInspector
    buildSelectedObjectInspectorData(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] QString activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] std::optional<std::uint32_t> activeTrailTargetBodyIndex(const SkySelectionOverlayInput& input) const;
};
