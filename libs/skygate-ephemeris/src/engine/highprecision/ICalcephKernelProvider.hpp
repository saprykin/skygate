#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <QtGlobal>

#include <optional>

namespace skygate::ephemeris::highprecision {

class ICalcephKernelProvider {
public:
    virtual ~ICalcephKernelProvider() = default;

    [[nodiscard]] virtual SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const
    {
        Q_UNUSED(epoch);
        Q_UNUSED(targetNaifId);
        Q_UNUSED(centerNaifId);

        SolarSystemKernelStateResult result;
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
        return result;
    }
};

}  // namespace skygate::ephemeris::highprecision
