#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

namespace skygate::ephemeris::highprecision {

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;
};

}  // namespace skygate::ephemeris::highprecision
