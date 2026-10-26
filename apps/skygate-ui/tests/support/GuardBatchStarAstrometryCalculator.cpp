#include "GuardBatchStarAstrometryCalculator.hpp"

#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"

#include <cstddef>
#include <vector>

namespace skygate::ui::tests {

skygate::ephemeris::highprecision::HighPrecisionCalculatorResult GuardBatchStarAstrometryCalculator::calculate(
    const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input
) const
{
    ++m_singleCallCount;
    skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result;
    result.equatorial = input.body.fixedEquatorialValue();
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.dataSourceProvenance = "performance guard single fallback";
    return result;
}

std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult>
GuardBatchStarAstrometryCalculator::calculateBatch(
    const skygate::ephemeris::EphemerisRequest& request,
    const skygate::ephemeris::highprecision::CatalogStarAstrometryArrays& arrays,
    std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState
) const
{
    static_cast<void>(request);
    static_cast<void>(preparedRequestState);

    ++m_batchCallCount;
    m_lastBatchSize = arrays.size();

    std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> results;
    results.reserve(arrays.size());
    for (std::size_t arrayIndex = 0; arrayIndex < arrays.size(); ++arrayIndex) {
        skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result;
        result.equatorial = arrays.referenceEquatorial(arrayIndex);
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.dataSourceProvenance = "performance guard batch astrometry";
        results.push_back(
            skygate::ephemeris::highprecision::StarAstrometryBatchResult{
                .bodyIndex = arrays.bodyIndices()[arrayIndex],
                .result = result,
            }
        );
    }
    return results;
}

int GuardBatchStarAstrometryCalculator::singleCallCount() const noexcept
{
    return m_singleCallCount;
}

int GuardBatchStarAstrometryCalculator::batchCallCount() const noexcept
{
    return m_batchCallCount;
}

std::size_t GuardBatchStarAstrometryCalculator::lastBatchSize() const noexcept
{
    return m_lastBatchSize;
}

}  // namespace skygate::ui::tests
