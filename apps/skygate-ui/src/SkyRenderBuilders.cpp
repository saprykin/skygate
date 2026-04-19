#include "SkyRenderBuilders.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace skygate::ui::internal;

namespace {

class HorizontalLookup final {
public:
    explicit HorizontalLookup(const skygate::ephemeris::SkySnapshot& snapshot)
    {
        m_horizontalByDisplayName.reserve(snapshot.states.size());
        m_horizontalByBodyId.reserve(snapshot.states.size());

        for (const auto& state : snapshot.states) {
            if (
                !std::isfinite(state.horizontal.altitudeDeg)
                || !std::isfinite(state.horizontal.azimuthDeg)
            ) {
                continue;
            }

            const auto& body = snapshot.bodyAt(state.bodyIndex);
            m_horizontalByBodyId[body.id] = state.horizontal;
            m_horizontalByDisplayName[body.displayName] = state.horizontal;
        }
    }

    [[nodiscard]] const skygate::core::HorizontalCoordinate* findHorizontal(
        const std::string_view bodyId
    ) const
    {
        if (const auto idIt = m_horizontalByBodyId.find(bodyId); idIt != m_horizontalByBodyId.end()) {
            return &idIt->second;
        }

        const std::string_view hipNumber = SkyContextRenderStyle::hipSuffix(bodyId);
        if (!hipNumber.empty()) {
            std::string legacyHipDisplayName = "HIP ";
            legacyHipDisplayName.append(hipNumber.data(), hipNumber.size());
            if (
                const auto displayNameIt = m_horizontalByDisplayName.find(legacyHipDisplayName);
                displayNameIt != m_horizontalByDisplayName.end()
            ) {
                return &displayNameIt->second;
            }
        }

        return nullptr;
    }

private:
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByBodyId;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByDisplayName;
};

struct DecimatedStarPoint final {
    SkyRenderPoint point;
    double visualMagnitude = 0.0;
    double distanceToCellCenterSquared = 0.0;
};

[[nodiscard]] QColor labelColorForBodyType(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return QColor(255, 224, 135, 235);
    case skygate::ephemeris::CelestialBodyType::Moon:
        return QColor(152, 247, 255, 245);
    case skygate::ephemeris::CelestialBodyType::Planet:
        return QColor(255, 196, 148, 235);
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return QColor(201, 220, 255, 230);
    case skygate::ephemeris::CelestialBodyType::Star:
        break;
    }

    return QColor(201, 220, 255, 230);
}

[[nodiscard]] double starDecimationCellSizePx(
    const skygate::core::ProjectionParams& projectionParams,
    const std::size_t stateCount,
    const double viewportWidth,
    const double viewportHeight
)
{
    if (stateCount < 30000U || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return 0.0;
    }

    if (projectionParams.fovDeg <= 35.0) {
        return 0.0;
    }

    const double normalizedFov = std::clamp((projectionParams.fovDeg - 35.0) / 95.0, 0.0, 1.0);
    const double baseCellSize = 2.5 + (normalizedFov * 4.5);
    const double viewportScale = std::clamp(
        std::sqrt((viewportWidth * viewportHeight) / (1100.0 * 760.0)),
        0.85,
        1.35
    );
    return baseCellSize * viewportScale;
}

[[nodiscard]] std::uint64_t screenCellKey(
    const double x,
    const double y,
    const double cellSizePx
)
{
    const std::uint32_t cellX = static_cast<std::uint32_t>(std::floor(x / cellSizePx));
    const std::uint32_t cellY = static_cast<std::uint32_t>(std::floor(y / cellSizePx));
    return (static_cast<std::uint64_t>(cellX) << 32U) | static_cast<std::uint64_t>(cellY);
}

[[nodiscard]] double distanceToCellCenterSquared(
    const double x,
    const double y,
    const double cellSizePx
)
{
    const double cellX = std::floor(x / cellSizePx);
    const double cellY = std::floor(y / cellSizePx);
    const double centerX = (cellX * cellSizePx) + (cellSizePx * 0.5);
    const double centerY = (cellY * cellSizePx) + (cellSizePx * 0.5);
    const double deltaX = x - centerX;
    const double deltaY = y - centerY;
    return (deltaX * deltaX) + (deltaY * deltaY);
}

[[nodiscard]] bool shouldReplaceStarPoint(
    const DecimatedStarPoint& existingPoint,
    const double visualMagnitude,
    const double nextDistanceToCellCenterSquared
)
{
    if (visualMagnitude < existingPoint.visualMagnitude) {
        return true;
    }

    if (std::abs(visualMagnitude - existingPoint.visualMagnitude) <= 1e-6) {
        return nextDistanceToCellCenterSquared < existingPoint.distanceToCellCenterSquared;
    }

    return false;
}

[[nodiscard]] SkyRenderPoint makeRenderPoint(
    const skygate::ephemeris::CelestialBody& body,
    const std::uint32_t bodyIndex,
    const skygate::core::ScreenPoint& projected
)
{
    SkyRenderPoint point;
    point.x = projected.x;
    point.y = projected.y;
    point.bodyIndex = bodyIndex;
    point.sizePx = SkyContextRenderStyle::pointSizeForMagnitude(body.visualMagnitude);
    if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
        point.sizePx = std::max(point.sizePx, 3.0);
    }
    point.color = SkyContextRenderStyle::colorForBodyType(body.type);
    return point;
}

void appendLabel(
    QVariantList& labels,
    const double x,
    const double y,
    const std::string_view text,
    const QColor& color
)
{
    QVariantMap labelEntry;
    labelEntry.insert("x", x);
    labelEntry.insert("y", y);
    labelEntry.insert("text", QString::fromStdString(std::string(text)));
    labelEntry.insert("color", color);
    labels.push_back(std::move(labelEntry));
}

}  // namespace

SkyRenderFrame SkyRenderFrameBuilder::buildFrame(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const double magnitudeCutoff,
    const double viewportWidth,
    const double viewportHeight
) const
{
    SkyRenderFrame frame;
    frame.lines.reserve(lineRefs.size());

    const double starCellSizePx = starDecimationCellSizePx(
        projection.params(),
        snapshot.states.size(),
        viewportWidth,
        viewportHeight
    );
    std::unordered_map<std::uint64_t, std::size_t> starPointIndexByCell;
    std::vector<DecimatedStarPoint> decimatedStarPoints;
    frame.points.reserve(snapshot.states.size() / 8U);
    if (starCellSizePx > 0.0) {
        starPointIndexByCell.reserve(snapshot.states.size() / 6U);
        decimatedStarPoints.reserve(snapshot.states.size() / 6U);
    }

    for (const auto& state : snapshot.states) {
        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
        ) {
            continue;
        }

        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (
            body.type == skygate::ephemeris::CelestialBodyType::Star
            && body.visualMagnitude > magnitudeCutoff
        ) {
            continue;
        }

        const auto projected = projection.project(state.horizontal);
        if (!projected.isVisible) {
            continue;
        }

        if (
            body.type == skygate::ephemeris::CelestialBodyType::Star
            && starCellSizePx > 0.0
        ) {
            const std::uint64_t cellKey = screenCellKey(projected.x, projected.y, starCellSizePx);
            const double cellCenterDistanceSquared = distanceToCellCenterSquared(
                projected.x,
                projected.y,
                starCellSizePx
            );
            if (const auto pointIndexIt = starPointIndexByCell.find(cellKey);
                pointIndexIt != starPointIndexByCell.end()) {
                auto& existingPoint = decimatedStarPoints[pointIndexIt->second];
                if (!shouldReplaceStarPoint(
                        existingPoint,
                        body.visualMagnitude,
                        cellCenterDistanceSquared
                    )) {
                    continue;
                }

                existingPoint.point = makeRenderPoint(body, state.bodyIndex, projected);
                existingPoint.visualMagnitude = body.visualMagnitude;
                existingPoint.distanceToCellCenterSquared = cellCenterDistanceSquared;
                continue;
            }

            starPointIndexByCell.emplace(cellKey, decimatedStarPoints.size());
            decimatedStarPoints.push_back(DecimatedStarPoint {
                .point = makeRenderPoint(body, state.bodyIndex, projected),
                .visualMagnitude = body.visualMagnitude,
                .distanceToCellCenterSquared = cellCenterDistanceSquared
            });
            continue;
        }

        frame.points.push_back(makeRenderPoint(body, state.bodyIndex, projected));
    }

    if (!decimatedStarPoints.empty()) {
        frame.points.reserve(frame.points.size() + decimatedStarPoints.size());
        for (auto& starPoint : decimatedStarPoints) {
            frame.points.push_back(std::move(starPoint.point));
        }
    }

    const HorizontalLookup lookup(snapshot);
    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.90;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;

    for (const auto& lineRef : lineRefs) {
        const auto* startHorizontal = lookup.findHorizontal(lineRef.first);
        const auto* endHorizontal = lookup.findHorizontal(lineRef.second);
        if (startHorizontal == nullptr || endHorizontal == nullptr) {
            continue;
        }

        const auto startProjected = projection.project(*startHorizontal);
        const auto endProjected = projection.project(*endHorizontal);
        if (!startProjected.isVisible || !endProjected.isVisible) {
            continue;
        }

        const double deltaX = endProjected.x - startProjected.x;
        const double deltaY = endProjected.y - startProjected.y;
        const double lengthSquared = deltaX * deltaX + deltaY * deltaY;
        if (lengthSquared > maxSegmentLengthSquared) {
            continue;
        }

        frame.lines.push_back(SkyRenderLine {
            .x1 = startProjected.x,
            .y1 = startProjected.y,
            .x2 = endProjected.x,
            .y2 = endProjected.y,
            .color = SkyContextRenderStyle::constellationLineColor()
        });
    }

    std::unordered_set<std::string_view> seenLabels;
    frame.labels.reserve(static_cast<int>(labelRefs.size() + 16U));
    constexpr double kEdgeMarginPx = 10.0;

    for (const auto& point : frame.points) {
        const auto& body = snapshot.bodyAt(point.bodyIndex);
        if (
            body.type != skygate::ephemeris::CelestialBodyType::Constellation
            && body.type != skygate::ephemeris::CelestialBodyType::Planet
            && body.type != skygate::ephemeris::CelestialBodyType::Sun
            && body.type != skygate::ephemeris::CelestialBodyType::Moon
        ) {
            continue;
        }

        if (body.displayName.empty() || !seenLabels.insert(body.displayName).second) {
            continue;
        }

        if (
            point.x < kEdgeMarginPx
            || point.x > (viewportWidth - kEdgeMarginPx)
            || point.y < kEdgeMarginPx
            || point.y > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        appendLabel(
            frame.labels,
            point.x,
            point.y,
            body.displayName,
            labelColorForBodyType(body.type)
        );
    }

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
            const auto* horizontal = lookup.findHorizontal(hipId);
            if (horizontal == nullptr) {
                continue;
            }

            const auto projected = projection.project(*horizontal);
            if (!projected.isVisible) {
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
        if (
            labelX < kEdgeMarginPx
            || labelX > (viewportWidth - kEdgeMarginPx)
            || labelY < kEdgeMarginPx
            || labelY > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        appendLabel(
            frame.labels,
            labelX,
            labelY,
            labelRef.first,
            labelColorForBodyType(skygate::ephemeris::CelestialBodyType::Constellation)
        );
    }

    return frame;
}
