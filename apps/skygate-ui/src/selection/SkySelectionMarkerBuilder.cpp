#include "SkySelectionMarkerBuilder.hpp"

#include "SkySceneShared.hpp"

#include "math/AngleMath.hpp"
#include "math/SphericalGeometry.hpp"

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

SkySelectionMarker selectionMarkerEntry(const double x, const double y)
{
    return SkySelectionMarker{.visible = true, .kind = "searchSelection", .x = x, .y = y};
}

bool hasSelectionInputs(const SkySelectionOverlayInput& input)
{
    return input.snapshot != nullptr && input.preparedProjection != nullptr && input.stateIndexByBodyId != nullptr;
}

std::optional<QPointF> selectedBodyPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& targetId
)
{
    const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedSceneLookupKey(targetId));
    if (stateIndexIt == stateIndexByBodyId.cend()) {
        return std::nullopt;
    }

    const auto& state = snapshot.states.at(*stateIndexIt);
    if (!state.horizontal.isFinite()) {
        return std::nullopt;
    }

    const auto projected = preparedProjection.project(state.horizontal);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

std::optional<QPointF> selectedConstellationPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedSceneLookupKey(targetId);
    if (normalizedTargetId.isEmpty()) {
        return std::nullopt;
    }

    const auto anchorGroupIt = std::find_if(
        anchorGroups.begin(),
        anchorGroups.end(),
        [&normalizedTargetId](const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup) {
            return normalizedSceneLookupKey(anchorGroup.first) == normalizedTargetId;
        }
    );
    if (anchorGroupIt == anchorGroups.end()) {
        return std::nullopt;
    }

    skygate::core::SphericalGeometry::Vector3d sum{0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : anchorGroupIt->second) {
        const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedSceneLookupKey(hipId));
        if (stateIndexIt == stateIndexByBodyId.cend()) {
            continue;
        }

        const auto& horizontal = snapshot.states.at(*stateIndexIt).horizontal;
        if (!horizontal.isFinite()) {
            continue;
        }

        const auto vector = skygate::core::SphericalGeometry::horizontalToUnitVector(horizontal);
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = skygate::core::SphericalGeometry::normalize(sum);
    if (skygate::core::SphericalGeometry::length(normalizedVector) <= 0.0) {
        return std::nullopt;
    }

    const skygate::core::HorizontalCoordinate center{
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(normalizedVector[2], -1.0, 1.0))),
        .azimuthDeg = skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AngleMath::toDegrees(std::atan2(normalizedVector[0], normalizedVector[1]))
        )
    };
    const auto projected = preparedProjection.project(center);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

}  // namespace

SkySelectionMarker SkySelectionMarkerBuilder::build(const SkySelectionOverlayInput& input) const
{
    if (!hasSelectionInputs(input)) {
        return {};
    }

    QString targetKind;
    QString targetId;
    if (!input.selectedObjectTargetId.isEmpty()) {
        targetKind = "body";
        targetId = input.selectedObjectTargetId;
    } else if (!input.trackedTargetId.trimmed().isEmpty()) {
        targetKind = input.trackedTargetKind;
        targetId = input.trackedTargetId;
    } else {
        targetKind = input.selectedSearchTargetKind;
        targetId = input.selectedSearchTargetId;
    }
    if (targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return {};
    }

    std::optional<QPointF> markerPoint;
    if (normalizedSceneLookupKey(targetKind) == "body") {
        markerPoint =
            selectedBodyPoint(*input.snapshot, *input.stateIndexByBodyId, *input.preparedProjection, targetId);
    } else if (normalizedSceneLookupKey(targetKind) == "constellationlabel") {
        markerPoint = selectedConstellationPoint(
            *input.snapshot,
            *input.stateIndexByBodyId,
            *input.preparedProjection,
            input.constellationAnchorGroups,
            targetId
        );
    }

    if (!markerPoint.has_value()) {
        return {};
    }

    return selectionMarkerEntry(markerPoint->x(), markerPoint->y());
}
