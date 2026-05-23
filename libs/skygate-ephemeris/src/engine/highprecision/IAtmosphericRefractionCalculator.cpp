#include "engine/highprecision/IAtmosphericRefractionCalculator.hpp"

namespace skygate::ephemeris::highprecision {

HighPrecisionCalculatorResult IAtmosphericRefractionCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    HighPrecisionCalculatorResult result = calculatorResult;
    if (input.request.options.enableAtmosphericRefraction
        && hasCorrectionFlag(input.request.options.correctionFlags, EphemerisCorrectionFlags::AtmosphericRefraction)) {
        result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::AtmosphericRefraction);
    }
    return result;
}

}  // namespace skygate::ephemeris::highprecision
