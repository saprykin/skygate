#pragma once

#include "ObservationContext.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "time/AstronomicalEpoch.hpp"

namespace skygate::ephemeris {

struct EphemerisRequest {
    // The explicit astronomical epoch is the authoritative computation
    // instant for request-based engine calls. Engines must use this epoch
    // rather than context.utcTime whenever epoch.hasExplicit() is true. When
    // no explicit epoch is present, callers should use the observation-context
    // convenience overloads or build an explicit UTC epoch from
    // context.utcTime via EphemerisRequestFactory::requestFromContext.
    skygate::core::AstronomicalEpoch epoch;
    skygate::core::ObservationContext context;
    EphemerisEngineOptions options;

    [[nodiscard]] static EphemerisRequest
    fromPrecisionPolicy(EphemerisRequest request, EphemerisPrecisionPolicy policy) noexcept;
};

}  // namespace skygate::ephemeris
