#pragma once

#include "ObservationContext.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "time/AstronomicalEpoch.hpp"

namespace skygate::ephemeris {

struct EphemerisRequest {
    AstronomicalEpoch epoch;
    skygate::core::ObservationContext context;
    EphemerisEngineOptions options;

    [[nodiscard]] static EphemerisRequest
    fromPrecisionPolicy(EphemerisRequest request, EphemerisPrecisionPolicy policy) noexcept;
};

}  // namespace skygate::ephemeris
