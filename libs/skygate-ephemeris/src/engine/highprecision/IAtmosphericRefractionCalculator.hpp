#pragma once

#include "HighPrecisionTypes.hpp"

namespace skygate::ephemeris::highprecision {

class IAtmosphericRefractionCalculator {
public:
    virtual ~IAtmosphericRefractionCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
