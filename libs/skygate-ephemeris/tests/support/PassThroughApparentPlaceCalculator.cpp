#include "PassThroughApparentPlaceCalculator.hpp"

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;

std::shared_ptr<IApparentPlaceCalculator> PassThroughApparentPlaceCalculator::create()
{
    return std::make_shared<PassThroughApparentPlaceCalculator>();
}

HighPrecisionCalculatorResult PassThroughApparentPlaceCalculator::apply(
    const HighPrecisionComputationInput&, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    return calculatorResult;
}

std::vector<StarAstrometryBatchResult> PassThroughApparentPlaceCalculator::applyBatch(
    const EphemerisRequest&,
    std::span<const BaseCelestialBody* const>,
    std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState>
) const
{
    return {calculatorResults.begin(), calculatorResults.end()};
}

}  // namespace skygate::ephemeris::tests
