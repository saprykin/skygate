#include "engine/highprecision/IApparentPlaceCalculator.hpp"

#include <QtGlobal>

#include <vector>

namespace skygate::ephemeris::highprecision {

std::vector<StarAstrometryBatchResult> IApparentPlaceCalculator::applyBatch(
    const EphemerisRequest& request,
    std::span<const CelestialBody> bodies,
    std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    Q_UNUSED(preparedRequestState);
    std::vector<StarAstrometryBatchResult> results;
    results.reserve(calculatorResults.size());
    for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        if (calculatorResult.bodyIndex >= bodies.size()) {
            continue;
        }
        const HighPrecisionComputationInput input{
            .request = request,
            .body = bodies[calculatorResult.bodyIndex],
            .preparedRequestState = preparedRequestState,
            .bodyIndex = calculatorResult.bodyIndex,
        };
        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = calculatorResult.bodyIndex,
                .result = apply(input, calculatorResult.result),
            }
        );
    }
    return results;
}

}  // namespace skygate::ephemeris::highprecision
