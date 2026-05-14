#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

namespace skygate::ephemeris::highprecision {

class AtmosphericRefractionCalculator final : public IAtmosphericRefractionCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override;
};

}  // namespace skygate::ephemeris::highprecision
