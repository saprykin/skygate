#include "SkyTrailTargetResolver.hpp"
#include "SkySelectionOverlayInput.hpp"
#include "SkySelectionTargetKind.hpp"

std::array<SkyTrailTargetResolver::Target, SkyTrailTargetResolver::kTargetCount>
SkyTrailTargetResolver::targetCandidates(const SkySelectionOverlayInput& input) const
{
    return {
        Target{
            .kind = skygate::ui::internal::SkySelectionTargetKind::token(
                skygate::ui::internal::SkySelectionTargetKind::Type::Body
            ),
            .id = input.selectedObjectTargetId,
        },
        Target{.kind = input.trackedTargetKind, .id = input.trackedTargetId},
        Target{.kind = input.selectedSearchTargetKind, .id = input.selectedSearchTargetId},
    };
}

QString SkyTrailTargetResolver::activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const
{
    for (const Target& candidate : targetCandidates(input)) {
        if (skygate::ui::internal::SkySelectionTargetKind::isBody(candidate.kind)
            && !candidate.id.trimmed().isEmpty()) {
            return candidate.id;
        }
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
