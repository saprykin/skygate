#pragma once

#include "PreparedProjection.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyRenderHorizontalLookup.hpp"
#include "SkyTheme.hpp"
#include "math/RectOccupancyGrid.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <span>
#include <string_view>
#include <unordered_set>

namespace skygate::ui::internal {

class SkyConstellationRenderBuilder final {
public:
    void appendLines(
        SkyRenderFrame& frame,
        const SkyRenderHorizontalLookup& horizontalLookup,
        const skygate::core::PreparedProjection& projection,
        std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme
    ) const;

    void appendLabels(
        SkyRenderFrame& frame,
        const SkyRenderHorizontalLookup& horizontalLookup,
        const skygate::core::PreparedProjection& projection,
        std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        double edgeMarginPx,
        std::unordered_set<std::string_view>& seenLabels,
        skygate::core::RectOccupancyGrid& labelGrid
    ) const;
};

}  // namespace skygate::ui::internal
