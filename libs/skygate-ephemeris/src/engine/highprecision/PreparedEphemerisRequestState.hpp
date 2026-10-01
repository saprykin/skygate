#pragma once

#include "EarthOrientationSampler.hpp"
#include "ICalcephKernel.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState {
    EphemerisEngineQueryResult tdbKernelEpochMetadata;
    std::optional<AstronomicalEpoch> tdbKernelEpoch;
    std::optional<ICalcephKernel::StateResult> annualParallaxEarthState;

    bool topocentricStatePrepared = false;
    bool topocentricStateAvailable = true;
    EphemerisEngineQueryResult topocentricMetadata;
    std::optional<EarthOrientationSampler::Sample> earthOrientationSample;
    std::optional<skygate::core::Vector3d> observerItrsPositionAu;
};

}  // namespace skygate::ephemeris::highprecision
