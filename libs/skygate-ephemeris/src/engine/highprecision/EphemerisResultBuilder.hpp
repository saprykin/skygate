#pragma once

#include "engine/highprecision/IEphemerisResultBuilder.hpp"

namespace skygate::ephemeris::highprecision {

class EphemerisResultBuilder final : public IEphemerisResultBuilder {
public:
    [[nodiscard]] CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override;

    [[nodiscard]] CelestialBodyState buildUnsupportedState(const HighPrecisionComputationInput& input) const override;
    [[nodiscard]] CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const override;
};

}  // namespace skygate::ephemeris::highprecision
