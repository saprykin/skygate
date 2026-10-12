#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyRenderGlyph.hpp"
#include "SkyRenderLabel.hpp"
#include "SkyThemeRenderPalette.hpp"
#include "math/Geometry2d.hpp"
#include "math/RectOccupancyGrid.hpp"

#include <QColor>

#include <string_view>
#include <unordered_set>
#include <vector>

namespace skygate::ui::internal {

class SkyRenderLabelBuilder final {
public:
    [[nodiscard]] static QColor
    colorForBodyType(skygate::ephemeris::BaseCelestialBody::Kind type, const SkyThemeRenderPalette& renderTheme);

    [[nodiscard]] static skygate::core::Rect2d bounds(double anchorX, double anchorY, std::string_view text);

    [[nodiscard]] static bool
    fitsViewport(const skygate::core::Rect2d& bounds, double viewportWidth, double viewportHeight, double edgeMarginPx);

    static void
    append(std::vector<SkyRenderLabel>& labels, QString kind, double x, double y, QString text, const QColor& color);

    static void
    append(std::vector<SkyRenderLabel>& labels, double x, double y, std::string_view text, const QColor& color);

    static void appendBodyPointLabels(
        SkyRenderFrame& frame,
        const skygate::ephemeris::EphemerisSnapshot& snapshot,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers,
        double edgeMarginPx,
        std::unordered_set<std::string_view>& seenLabels,
        skygate::core::RectOccupancyGrid& labelGrid
    );

    static void appendDeepSkyLabels(
        SkyRenderFrame& frame,
        const skygate::ephemeris::EphemerisSnapshot& snapshot,
        const skygate::core::ProjectionParams& projectionParams,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        double edgeMarginPx,
        std::unordered_set<std::string_view>& seenLabels,
        skygate::core::RectOccupancyGrid& labelGrid
    );
};

}  // namespace skygate::ui::internal
