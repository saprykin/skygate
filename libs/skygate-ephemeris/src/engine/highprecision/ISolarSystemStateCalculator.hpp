#pragma once

#include "HighPrecisionTypes.hpp"

namespace skygate::ephemeris::highprecision {

class ISolarSystemStateCalculator {
public:
    virtual ~ISolarSystemStateCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
