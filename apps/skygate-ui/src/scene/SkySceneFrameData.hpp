#pragma once

#include "EphemerisSnapshot.hpp"
#include "PreparedProjection.hpp"
#include "SkyOverlayItem.hpp"
#include "SkySelectedObjectInspector.hpp"
#include "SkySelectionMarker.hpp"
#include "render/SkyRenderFrame.hpp"

#include <optional>
#include <vector>

struct SkySceneFrameData final {
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    const skygate::ephemeris::EphemerisSnapshot* snapshot = nullptr;
    SkyRenderFrame frame;
    std::vector<SkyOverlayItem> overlayItems;
    SkySelectionMarker selectionMarker;
    SkySelectedObjectInspector selectedObjectInspector;
};
