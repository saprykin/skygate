#include "SkyConstellationRenderBuilder.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyRenderLabels.hpp"

#include "skygate/core/math/ProjectedPolylineBuilder.hpp"

#include <algorithm>
#include <array>

namespace {

constexpr double kConstellationLineWidthPx = 1.8;

}  // namespace

namespace skygate::ui::internal {

void SkyConstellationRenderBuilder::appendLines(
    SkyRenderFrame& frame,
    const SkyRenderHorizontalLookup& horizontalLookup,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme
) const
{
    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.90;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    const skygate::core::ProjectedPolylineBuilder lineBuilder;
    const QColor lineColor = SkyContextRenderStyle::constellationLineColor(renderTheme);

    for (const auto& lineRef : lineRefs) {
        const auto* startHorizontal = horizontalLookup.findHorizontal(lineRef.first);
        const auto* endHorizontal = horizontalLookup.findHorizontal(lineRef.second);
        if (startHorizontal == nullptr || endHorizontal == nullptr) {
            continue;
        }

        const std::array<skygate::core::HorizontalCoordinate, 2> coordinates {
            *startHorizontal,
            *endHorizontal
        };
        for (const auto& segment : lineBuilder.build(
                 projection,
                 coordinates,
                 maxSegmentLengthSquared
             )) {
            frame.lines.push_back(SkyRenderLine {
                .x1 = segment.x1,
                .y1 = segment.y1,
                .x2 = segment.x2,
                .y2 = segment.y2,
                .widthPx = kConstellationLineWidthPx,
                .color = lineColor
            });
        }
    }
}

void SkyConstellationRenderBuilder::appendLabels(
    SkyRenderFrame& frame,
    const SkyRenderHorizontalLookup& horizontalLookup,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
) const
{
    for (const auto& labelRef : labelRefs) {
        if (labelRef.first.empty() || labelRef.second.empty()) {
            continue;
        }

        if (!seenLabels.insert(labelRef.first).second) {
            continue;
        }

        double sumX = 0.0;
        double sumY = 0.0;
        int visiblePointCount = 0;
        for (const std::string& hipId : labelRef.second) {
            const auto* horizontal = horizontalLookup.findHorizontal(hipId);
            if (horizontal == nullptr) {
                continue;
            }

            const auto projected = projection.project(*horizontal);
            if (!projected.isVisible || !projected.isFinite()) {
                continue;
            }

            sumX += projected.x;
            sumY += projected.y;
            ++visiblePointCount;
        }

        if (visiblePointCount == 0) {
            continue;
        }

        const double labelX = sumX / static_cast<double>(visiblePointCount);
        const double labelY = sumY / static_cast<double>(visiblePointCount);
        const skygate::core::Rect2d bounds = skyRenderLabelBounds(labelX, labelY, labelRef.first);
        if (
            !skyRenderLabelFitsViewport(bounds, viewportWidth, viewportHeight, edgeMarginPx)
            || labelGrid.collides(bounds)
        ) {
            continue;
        }

        appendSkyRenderLabel(
            frame.labels,
            labelX,
            labelY,
            labelRef.first,
            skyRenderLabelColorForBodyType(
                skygate::ephemeris::CelestialBodyType::Constellation,
                renderTheme
            )
        );
        labelGrid.add(bounds);
    }
}

}  // namespace skygate::ui::internal
