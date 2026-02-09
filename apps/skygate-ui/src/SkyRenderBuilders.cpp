#include "SkyRenderBuilders.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QColor>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

using namespace skygate::ui::internal;

namespace {

class HorizontalLookup final {
public:
    explicit HorizontalLookup(const skygate::ephemeris::SkySnapshot& snapshot)
    {
        m_horizontalById.reserve(snapshot.states.size());
        m_horizontalByDisplayName.reserve(snapshot.states.size());

        for (const auto& state : snapshot.states) {
            if (
                !std::isfinite(state.horizontal.altitudeDeg)
                || !std::isfinite(state.horizontal.azimuthDeg)
            ) {
                continue;
            }

            m_horizontalById[state.body.id] = state.horizontal;
            m_horizontalByDisplayName[state.body.displayName] = state.horizontal;
        }
    }

    [[nodiscard]] const skygate::core::HorizontalCoordinate* findHorizontal(
        const std::string_view bodyId
    ) const
    {
        if (const auto idIt = m_horizontalById.find(bodyId); idIt != m_horizontalById.end()) {
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
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalById;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByDisplayName;
};

QColor labelColorForBodyType(const skygate::ephemeris::CelestialBodyType type)
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

}  // namespace

std::vector<SkyContextController::SkyRenderPoint> SkyRenderPointBuilder::buildPoints(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::IProjection& projection,
    const skygate::core::ProjectionParams& projectionParams,
    const double magnitudeCutoff
) const
{
    std::vector<SkyContextController::SkyRenderPoint> points;
    points.reserve(snapshot.states.size());

    for (const auto& state : snapshot.states) {
        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
        ) {
            continue;
        }

        if (
            state.body.type == skygate::ephemeris::CelestialBodyType::Star
            && state.body.visualMagnitude > magnitudeCutoff
        ) {
            continue;
        }

        const auto projected = projection.project(state.horizontal, projectionParams);
        if (!projected.isVisible) {
            continue;
        }

        SkyContextController::SkyRenderPoint point;
        point.x = projected.x;
        point.y = projected.y;
        point.sizePx = SkyContextRenderStyle::pointSizeForMagnitude(state.body.visualMagnitude);
        if (state.body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            point.sizePx = std::max(point.sizePx, 3.0);
        }
        point.displayName = QString::fromStdString(state.body.displayName);
        point.color = SkyContextRenderStyle::colorForBodyType(state.body.type);
        points.push_back(std::move(point));
    }

    return points;
}

std::vector<SkyContextController::SkyRenderLine> SkyConstellationRenderBuilder::buildLines(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::IProjection& projection,
    const skygate::core::ProjectionParams& projectionParams,
    const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs,
    const double viewportWidth,
    const double viewportHeight
) const
{
    const HorizontalLookup lookup(snapshot);

    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.90;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;

    std::vector<SkyContextController::SkyRenderLine> lines;
    lines.reserve(lineRefs.size());

    for (const auto& lineRef : lineRefs) {
        const auto* startHorizontal = lookup.findHorizontal(lineRef.first);
        const auto* endHorizontal = lookup.findHorizontal(lineRef.second);
        if (startHorizontal == nullptr || endHorizontal == nullptr) {
            continue;
        }

        const auto startProjected = projection.project(*startHorizontal, projectionParams);
        const auto endProjected = projection.project(*endHorizontal, projectionParams);
        if (!startProjected.isVisible || !endProjected.isVisible) {
            continue;
        }

        const double deltaX = endProjected.x - startProjected.x;
        const double deltaY = endProjected.y - startProjected.y;
        const double lengthSquared = deltaX * deltaX + deltaY * deltaY;
        if (lengthSquared > maxSegmentLengthSquared) {
            continue;
        }

        SkyContextController::SkyRenderLine line;
        line.x1 = startProjected.x;
        line.y1 = startProjected.y;
        line.x2 = endProjected.x;
        line.y2 = endProjected.y;
        line.color = SkyContextRenderStyle::constellationLineColor();
        lines.push_back(std::move(line));
    }

    return lines;
}

QVariantList SkyConstellationRenderBuilder::buildLabels(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::IProjection& projection,
    const skygate::core::ProjectionParams& projectionParams,
    const std::vector<skygate::ephemeris::ConstellationLabelRef>& labelRefs,
    const double viewportWidth,
    const double viewportHeight
) const
{
    const HorizontalLookup lookup(snapshot);

    std::unordered_set<std::string> seenLabels;
    QVariantList labels;
    labels.reserve(static_cast<int>((snapshot.states.size() / 8U) + labelRefs.size()));

    constexpr double kEdgeMarginPx = 10.0;

    for (const auto& state : snapshot.states) {
        if (
            state.body.type != skygate::ephemeris::CelestialBodyType::Constellation
            && state.body.type != skygate::ephemeris::CelestialBodyType::Planet
            && state.body.type != skygate::ephemeris::CelestialBodyType::Sun
            && state.body.type != skygate::ephemeris::CelestialBodyType::Moon
        ) {
            continue;
        }

        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
            || state.body.displayName.empty()
        ) {
            continue;
        }

        if (!seenLabels.insert(state.body.displayName).second) {
            continue;
        }

        const auto projected = projection.project(state.horizontal, projectionParams);
        if (!projected.isVisible) {
            continue;
        }

        if (
            projected.x < kEdgeMarginPx
            || projected.x > (viewportWidth - kEdgeMarginPx)
            || projected.y < kEdgeMarginPx
            || projected.y > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        QVariantMap labelEntry;
        labelEntry.insert("x", projected.x);
        labelEntry.insert("y", projected.y);
        labelEntry.insert("text", QString::fromStdString(state.body.displayName));
        labelEntry.insert("color", labelColorForBodyType(state.body.type));
        labels.push_back(labelEntry);
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

            const auto projected = projection.project(*horizontal, projectionParams);
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

        QVariantMap labelEntry;
        labelEntry.insert("x", labelX);
        labelEntry.insert("y", labelY);
        labelEntry.insert("text", QString::fromStdString(labelRef.first));
        labelEntry.insert(
            "color",
            labelColorForBodyType(skygate::ephemeris::CelestialBodyType::Constellation)
        );
        labels.push_back(labelEntry);
    }

    return labels;
}
