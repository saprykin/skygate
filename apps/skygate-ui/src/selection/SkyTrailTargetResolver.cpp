#include "SkyTrailTargetResolver.hpp"

#include "SkySceneShared.hpp"

QString SkyTrailTargetResolver::activeTrailTargetBodyId(
    const SkySelectionOverlayInput& input
) const
{
    if (!input.selectedObjectTargetId.trimmed().isEmpty()) {
        return input.selectedObjectTargetId;
    }

    if (
        normalizedSceneLookupKey(input.trackedTargetKind) == "body"
        && !input.trackedTargetId.trimmed().isEmpty()
    ) {
        return input.trackedTargetId;
    }

    if (
        normalizedSceneLookupKey(input.selectedSearchTargetKind) == "body"
        && !input.selectedSearchTargetId.trimmed().isEmpty()
    ) {
        return input.selectedSearchTargetId;
    }

    return {};
}

std::optional<std::uint32_t> SkyTrailTargetResolver::activeTrailTargetBodyIndex(
    const SkySelectionOverlayInput& input
) const
{
    if (input.snapshot == nullptr || input.stateIndexByBodyId == nullptr) {
        return std::nullopt;
    }

    const QString targetBodyId = activeTrailTargetBodyId(input);
    if (targetBodyId.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const auto stateIndexIt = input.stateIndexByBodyId->constFind(
        normalizedSceneLookupKey(targetBodyId)
    );
    if (stateIndexIt == input.stateIndexByBodyId->cend()) {
        return std::nullopt;
    }

    return input.snapshot->states.at(*stateIndexIt).bodyIndex;
}
