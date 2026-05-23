#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

namespace skygate::ephemeris::highprecision {

class ICalcephKernelProvider {
public:
    virtual ~ICalcephKernelProvider() = default;

    [[nodiscard]] virtual SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
