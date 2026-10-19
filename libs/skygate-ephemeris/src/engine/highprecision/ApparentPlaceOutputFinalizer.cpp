#include "ApparentPlaceOutputFinalizer.hpp"
#include "CelestialFrameMath.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "HighPrecisionComputationInput.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris::highprecision {

ApparentPlaceOutputFinalizer::ApparentPlaceOutputFinalizer(
    const IAtmosphericRefractionCalculator* atmosphericRefractionCalculator
)
    : m_atmosphericRefractionCalculator(atmosphericRefractionCalculator)
{
}

std::vector<StarAstrometryBatchResult> ApparentPlaceOutputFinalizer::finalize(
    const EphemerisRequest& request,
    const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState,
    const std::span<FrameTransformBatcher::Item> items
) const
{
    std::vector<StarAstrometryBatchResult> results;
    results.reserve(items.size());
    for (FrameTransformBatcher::Item& item : items) {
        const bool canApplyRefraction = finalizeEquatorialCoordinate(item);
        applyAtmosphericRefraction(item, canApplyRefraction, request, preparedRequestState);

        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = item.bodyIndex,
                .result = std::move(item.result),
            }
        );
    }

    return results;
}

bool ApparentPlaceOutputFinalizer::finalizeEquatorialCoordinate(FrameTransformBatcher::Item& item)
{
    if (!item.equatorialVector.has_value()) {
        return false;
    }

    if (const std::optional<skygate::core::EquatorialCoordinate> equatorial =
            CelestialFrameMath::toEquatorial(*item.equatorialVector);
        equatorial.has_value()) {
        item.result.equatorial = *equatorial;
        return true;
    }

    item.result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    item.result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);

    return false;
}

void ApparentPlaceOutputFinalizer::applyAtmosphericRefraction(
    FrameTransformBatcher::Item& item,
    const bool canApplyRefraction,
    const EphemerisRequest& request,
    const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState
) const
{
    if (!request.options.requestsAtmosphericRefraction() || !canApplyRefraction) {
        return;
    }

    if (m_atmosphericRefractionCalculator == nullptr) {
        EphemerisMetadataMerger::markCorrectionUnavailable(
            item.result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
        );
        return;
    }

    const HighPrecisionComputationInput input{
        .request = request,
        .body = *item.body,
        .preparedRequestState = preparedRequestState,
        .bodyIndex = item.bodyIndex,
    };
    item.result = m_atmosphericRefractionCalculator->apply(input, item.result);
}

}  // namespace skygate::ephemeris::highprecision
