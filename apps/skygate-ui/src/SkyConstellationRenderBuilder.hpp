#pragma once

#include "SkyRenderFrame.hpp"
#include "SkyRenderHorizontalLookup.hpp"
#include "SkyTheme.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/math/SpatialIndex2d.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"

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
        std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        double edgeMarginPx,
        std::unordered_set<std::string_view>& seenLabels,
        skygate::core::RectOccupancyGrid& labelGrid
    ) const;
};

}  // namespace skygate::ui::internal
