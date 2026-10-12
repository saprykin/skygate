#pragma once

#include "SkySelectedObjectInspector.hpp"
#include "SkySelectionMarker.hpp"
#include "SkySelectionOverlayInput.hpp"

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
