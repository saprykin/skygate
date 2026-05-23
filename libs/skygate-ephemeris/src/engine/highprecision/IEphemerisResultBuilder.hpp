#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

namespace skygate::ephemeris::highprecision {

class IEphemerisResultBuilder {
public:
    virtual ~IEphemerisResultBuilder() = default;

    [[nodiscard]] virtual CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const = 0;

    [[nodiscard]] virtual CelestialBodyState
    buildUnsupportedState(const HighPrecisionComputationInput& input) const = 0;

    [[nodiscard]] virtual CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
