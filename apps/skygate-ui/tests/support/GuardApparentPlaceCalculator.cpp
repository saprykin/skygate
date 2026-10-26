#include "GuardApparentPlaceCalculator.hpp"

#include "HorizontalCoordinate.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace skygate::ui::tests {

skygate::ephemeris::highprecision::HighPrecisionCalculatorResult GuardApparentPlaceCalculator::apply(
    const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input,
    const skygate::ephemeris::highprecision::HighPrecisionCalculatorResult& calculatorResult
) const
{
    ++m_singleCallCount;
    skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result = calculatorResult;
    result.horizontal = horizontalForBodyIndex(input.bodyIndex);
    result.metadata.appliedCorrections = input.request.options.correctionFlags();
    return result;
}

std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> GuardApparentPlaceCalculator::applyBatch(
    const skygate::ephemeris::EphemerisRequest& request,
    std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies,
    std::span<const skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState
) const
{
    static_cast<void>(bodies);
    static_cast<void>(preparedRequestState);

    ++m_batchCallCount;
    m_lastBatchSize = calculatorResults.size();

    std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> results;
    results.reserve(calculatorResults.size());
    for (const skygate::ephemeris::highprecision::StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result = calculatorResult.result;
        result.horizontal = horizontalForBodyIndex(calculatorResult.bodyIndex);
        result.metadata.appliedCorrections = request.options.correctionFlags();
        results.push_back(
            skygate::ephemeris::highprecision::StarAstrometryBatchResult{
                .bodyIndex = calculatorResult.bodyIndex,
                .result = result,
            }
        );
    }
    return results;
}

int GuardApparentPlaceCalculator::singleCallCount() const noexcept
{
    return m_singleCallCount;
}

int GuardApparentPlaceCalculator::batchCallCount() const noexcept
{
    return m_batchCallCount;
}

std::size_t GuardApparentPlaceCalculator::lastBatchSize() const noexcept
{
    return m_lastBatchSize;
}

skygate::core::HorizontalCoordinate
GuardApparentPlaceCalculator::horizontalForBodyIndex(const std::size_t bodyIndex) noexcept
{
    return skygate::core::HorizontalCoordinate{
        .altitudeDeg = -20.0 + static_cast<double>(bodyIndex % 90U),
        .azimuthDeg = std::fmod(static_cast<double>(bodyIndex) * 0.37, 360.0),
    };
}

}  // namespace skygate::ui::tests
