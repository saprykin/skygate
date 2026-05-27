#pragma once

#include "SkyContext.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "time/AstronomicalEpoch.hpp"

namespace skygate::ephemeris {

struct EphemerisRequest {
    AstronomicalEpoch epoch;
    core::SkyContext context;
    EphemerisEngineOptions options;

    [[nodiscard]] static EphemerisRequest
    fromPrecisionPolicy(EphemerisRequest request, EphemerisPrecisionPolicy policy) noexcept;
};

}  // namespace skygate::ephemeris
