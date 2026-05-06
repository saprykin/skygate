#include "SkyRenderBuilders.hpp"

#include "SkyConstellationRenderBuilder.hpp"
#include "SkyRenderBodyBuilder.hpp"
#include "SkyRenderHorizontalLookup.hpp"
#include "SkyRenderLabels.hpp"

#include <optional>
#include <string_view>
#include <unordered_set>

using namespace skygate::ui::internal;

SkyRenderFrame SkyRenderFrameBuilder::buildFrame(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const double magnitudeCutoff,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers
) const
{
    SkyRenderFrame frame;
    frame.lines.reserve(lineRefs.size());

    std::optional<SkyRenderHorizontalLookup> horizontalLookup;
    if (!lineRefs.empty() || !labelRefs.empty()) {
        horizontalLookup.emplace(lineRefs, labelRefs);
    }

    const SkyRenderBodyBuilder bodyBuilder;
    bodyBuilder.appendBodies(
        frame,
        horizontalLookup.has_value() ? &*horizontalLookup : nullptr,
        snapshot,
        projection,
        magnitudeCutoff,
        viewportWidth,
        viewportHeight,
        renderTheme,
        overlayLayers
    );

    const SkyConstellationRenderBuilder constellationBuilder;
    if (horizontalLookup.has_value() && overlayLayers.constellationLines) {
        constellationBuilder.appendLines(
            frame,
            *horizontalLookup,
            projection,
            lineRefs,
            viewportWidth,
            viewportHeight,
            renderTheme
        );
    }

    std::unordered_set<std::string_view> seenLabels;
    frame.labels.reserve(labelRefs.size() + 16U);
    constexpr double kEdgeMarginPx = 10.0;
    skygate::core::RectOccupancyGrid labelGrid(72.0);

    appendBodyPointLabels(
        frame,
        snapshot,
        viewportWidth,
        viewportHeight,
        renderTheme,
        overlayLayers,
        kEdgeMarginPx,
        seenLabels,
        labelGrid
    );

    if (overlayLayers.deepSkyLabels) {
        appendDeepSkyLabels(
            frame,
            snapshot,
            projection.params(),
            viewportWidth,
            viewportHeight,
            renderTheme,
            kEdgeMarginPx,
            seenLabels,
            labelGrid
        );
    }

    if (horizontalLookup.has_value() && overlayLayers.constellationLabels) {
        constellationBuilder.appendLabels(
            frame,
            *horizontalLookup,
            projection,
            labelRefs,
            viewportWidth,
            viewportHeight,
            renderTheme,
            kEdgeMarginPx,
            seenLabels,
            labelGrid
        );
    }

    return frame;
}
