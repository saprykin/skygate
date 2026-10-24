#include "SkySelectionMarkerBuilder.hpp"
#include "ConstellationReferenceCalculator.hpp"
#include "SkySelectionOverlayInput.hpp"
#include "SkySelectionTargetKind.hpp"
#include "SkyTrailTargetResolver.hpp"
#include "scene/SkySceneLookupKey.hpp"
#include "scene/SkySelectionMarker.hpp"

#include <QPointF>

#include <algorithm>
#include <array>
#include <optional>
#include <string>

namespace {

SkySelectionMarker selectionMarkerEntry(const double x, const double y)
{
    return SkySelectionMarker{.visible = true, .kind = "searchSelection", .x = x, .y = y};
}

std::optional<QPointF> selectedBodyPoint(const SkySelectionOverlayInput& input, const QString& targetId)
{
    const auto stateIndex = input.stateIndexForBodyId(targetId);
    if (!stateIndex.has_value()) {
        return std::nullopt;
    }

    const auto& state = input.snapshot->states.at(*stateIndex);
    if (!state.horizontal.isFinite()) {
        return std::nullopt;
    }

    const auto projected = input.preparedProjection->project(state.horizontal);
    if (!projected.isVisible()) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

std::optional<QPointF> selectedConstellationPoint(const SkySelectionOverlayInput& input, const QString& targetId)
{
    const QString normalizedTargetId = skygate::ui::internal::SkySceneLookupKey::normalized(targetId);
    if (normalizedTargetId.isEmpty()) {
        return std::nullopt;
    }

    const auto anchorGroupIt = std::find_if(
        input.constellationAnchorGroups.begin(),
        input.constellationAnchorGroups.end(),
        [&normalizedTargetId](const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup) {
            return skygate::ui::internal::SkySceneLookupKey::normalized(anchorGroup.first) == normalizedTargetId;
        }
    );
    if (anchorGroupIt == input.constellationAnchorGroups.end()) {
        return std::nullopt;
    }

    skygate::ephemeris::ConstellationReferenceCalculator::AnchorCentroidAccumulator accumulator;
    for (const std::string& hipId : anchorGroupIt->second) {
        const auto stateIndex = input.stateIndexForBodyId(QString::fromStdString(hipId));
        if (!stateIndex.has_value()) {
            continue;
        }

        accumulator.addAnchor(input.snapshot->states.at(*stateIndex).horizontal);
    }

    const auto center = accumulator.centroid();
    if (!center.has_value()) {
        return std::nullopt;
    }

    const auto projected = input.preparedProjection->project(*center);
    if (!projected.isVisible()) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

}  // namespace

SkySelectionMarker SkySelectionMarkerBuilder::build(const SkySelectionOverlayInput& input) const
{
    if (!input.hasRequiredSnapshotInputs()) {
        return {};
    }

    // The marker follows the target precedence of the trail resolver and keeps
    // the target kind, so it can place both bodies and constellation labels.
    const SkyTrailTargetResolver targetResolver;
    const std::array<SkyTrailTargetResolver::Target, SkyTrailTargetResolver::kTargetCount> targetCandidates =
        targetResolver.targetCandidates(input);
    const auto targetIt = std::find_if(
        targetCandidates.begin(), targetCandidates.end(), [](const SkyTrailTargetResolver::Target& candidate) {
            return !candidate.id.isEmpty();
        }
    );
    if (targetIt == targetCandidates.end()) {
        return {};
    }

    const QString targetKind = targetIt->kind;
    const QString targetId = targetIt->id;
    if (targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return {};
    }

    std::optional<QPointF> markerPoint;
    if (skygate::ui::internal::SkySelectionTargetKind::isBody(targetKind)) {
        markerPoint = selectedBodyPoint(input, targetId);
    } else if (skygate::ui::internal::SkySelectionTargetKind::isConstellationLabel(targetKind)) {
        markerPoint = selectedConstellationPoint(input, targetId);
    }

    if (!markerPoint.has_value()) {
        return {};
    }

    return selectionMarkerEntry(markerPoint->x(), markerPoint->y());
}
