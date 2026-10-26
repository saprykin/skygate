#include "SkyConstellationRenderBuilder.hpp"
#include "SkyRenderLabelBuilder.hpp"
#include "SkyRenderLine.hpp"
#include "math/ProjectedPolylineBuilder.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <array>

namespace {

constexpr double kConstellationLineWidthPx = 1.8;
// Constellation lines may span the whole sky, so they clip at a larger share of
// the viewport than the rest of the overlay geometry.
constexpr double kConstellationSegmentLengthViewportFraction = 0.90;

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
    const double maxSegmentLengthSquared = skygate::core::ProjectedPolylineBuilder::viewportSegmentLimitSquared(
        viewportWidth, viewportHeight, kConstellationSegmentLengthViewportFraction
    );
    const skygate::core::ProjectedPolylineBuilder lineBuilder;
    const QColor lineColor = renderTheme.constellationLine;

    for (const auto& lineRef : lineRefs) {
        const auto* startHorizontal = horizontalLookup.findHorizontal(lineRef.first);
        const auto* endHorizontal = horizontalLookup.findHorizontal(lineRef.second);
        if (startHorizontal == nullptr || endHorizontal == nullptr) {
            continue;
        }

        const std::array<skygate::core::HorizontalCoordinate, 2> coordinates{*startHorizontal, *endHorizontal};
        for (const auto& segment : lineBuilder.build(projection, coordinates, maxSegmentLengthSquared)) {
            frame.lines.push_back(
                SkyRenderLine{
                    .x1 = segment.x1,
                    .y1 = segment.y1,
                    .x2 = segment.x2,
                    .y2 = segment.y2,
                    .widthPx = kConstellationLineWidthPx,
                    .color = lineColor
                }
            );
        }
    }
}

void SkyConstellationRenderBuilder::appendLabels(
    SkyRenderFrame& frame,
    const SkyRenderHorizontalLookup& horizontalLookup,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
) const
{
    for (const auto& anchorGroup : anchorGroups) {
        if (anchorGroup.first.empty() || anchorGroup.second.empty()) {
            continue;
        }

        if (!seenLabels.insert(anchorGroup.first).second) {
            continue;
        }

        // Label placement averages the projected anchors that are on screen, so
        // the label sits among the drawn anchor points. That is deliberately not
        // ConstellationReferenceCalculator's direction centroid, which the
        // selection marker uses to mark the constellation's sky position: the
        // two differ when only part of the constellation is visible.
        double sumX = 0.0;
        double sumY = 0.0;
        int visiblePointCount = 0;
        for (const std::string& hipId : anchorGroup.second) {
            const auto* horizontal = horizontalLookup.findHorizontal(hipId);
            if (horizontal == nullptr) {
                continue;
            }

            const auto projected = projection.project(*horizontal);
            if (!projected.isVisible() || !projected.isFinite()) {
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
        const skygate::core::Rect2d labelBounds = SkyRenderLabelBuilder::bounds(labelX, labelY, anchorGroup.first);
        if (!SkyRenderLabelBuilder::fitsViewport(labelBounds, viewportWidth, viewportHeight, edgeMarginPx)
            || labelGrid.collides(labelBounds)) {
            continue;
        }

        SkyRenderLabelBuilder::append(
            frame.labels,
            labelX,
            labelY,
            anchorGroup.first,
            SkyRenderLabelBuilder::colorForBodyType(
                skygate::ephemeris::BaseCelestialBody::Kind::Constellation, renderTheme
            )
        );
        labelGrid.add(labelBounds);
    }
}

}  // namespace skygate::ui::internal
