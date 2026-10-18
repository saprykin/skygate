#pragma once

#include "ObservationContext.hpp"
#include "PreparedProjection.hpp"
#include "scene/SkyOverlayItem.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <vector>

namespace skygate::ui::internal {

// Places the label of each reference line at the visible sample closest to a
// preferred spot, falling back to the closest sample when none clears the
// viewport edge margin.
class SkyReferenceOverlayLabelBuilder final {
public:
    SkyReferenceOverlayLabelBuilder() = delete;

    static void appendLabels(
        std::vector<SkyOverlayItem>& overlayItems,
        const skygate::core::PreparedProjection& preparedProjection,
        const SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers,
        const skygate::core::ObservationContext& skyContext
    );
};

}  // namespace skygate::ui::internal
