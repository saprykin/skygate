#include "SkyTrailTargetResolver.hpp"
#include "SkySelectionOverlayInput.hpp"
#include "SkySelectionTargetKind.hpp"

QString SkyTrailTargetResolver::activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const
{
    if (!input.selectedObjectTargetId.trimmed().isEmpty()) {
        return input.selectedObjectTargetId;
    }

    if (skygate::ui::internal::SkySelectionTargetKind::isBody(input.trackedTargetKind)
        && !input.trackedTargetId.trimmed().isEmpty()) {
        return input.trackedTargetId;
    }

    if (skygate::ui::internal::SkySelectionTargetKind::isBody(input.selectedSearchTargetKind)
        && !input.selectedSearchTargetId.trimmed().isEmpty()) {
        return input.selectedSearchTargetId;
    }

    return {};
}

std::optional<std::uint32_t>
SkyTrailTargetResolver::activeTrailTargetBodyIndex(const SkySelectionOverlayInput& input) const
{
    if (input.snapshot == nullptr) {
        return std::nullopt;
    }

    const QString targetBodyId = activeTrailTargetBodyId(input);
    if (targetBodyId.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const auto stateIndex = input.stateIndexForBodyId(targetBodyId);
    if (!stateIndex.has_value()) {
        return std::nullopt;
    }

    return input.snapshot->states.at(*stateIndex).bodyIndex;
}
