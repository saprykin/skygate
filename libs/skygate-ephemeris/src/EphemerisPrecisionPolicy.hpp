#pragma once

#include "Types.hpp"

namespace skygate::ephemeris {

enum class EphemerisPrecisionPolicy : std::uint8_t {
    SceneRender,
    SelectionDetail,
    Trail,
    EventSearch,
    NightConditionsApproximate,
    NightConditionsVerified
};

// Full-scene rendering and selected-object trails intentionally use a lean
// topocentric request in high-precision mode so broad redraws stay interactive.
// Selection details, event search, and verified night-condition calculations
// keep the caller's complete correction set.
[[nodiscard]] constexpr EphemerisRequest
ephemerisRequestForPrecisionPolicy(EphemerisRequest request, const EphemerisPrecisionPolicy policy) noexcept
{
    if (request.options.engineKind != EphemerisEngineKind::HighPrecision) {
        return request;
    }

    switch (policy) {
    case EphemerisPrecisionPolicy::SceneRender:
    case EphemerisPrecisionPolicy::Trail: {
        EphemerisCorrectionFlags corrections = EphemerisCorrectionFlags::PrecessionNutation
                                               | EphemerisCorrectionFlags::EarthOrientation
                                               | EphemerisCorrectionFlags::DiurnalParallax;
        if (request.options.enableAtmosphericRefraction
            && hasCorrectionFlag(request.options.correctionFlags, EphemerisCorrectionFlags::AtmosphericRefraction)) {
            corrections |= EphemerisCorrectionFlags::AtmosphericRefraction;
        }
        request.options.correctionFlags = corrections;
        return request;
    }
    case EphemerisPrecisionPolicy::SelectionDetail:
    case EphemerisPrecisionPolicy::EventSearch:
    case EphemerisPrecisionPolicy::NightConditionsApproximate:
    case EphemerisPrecisionPolicy::NightConditionsVerified:
        return request;
    }

    return request;
}

}  // namespace skygate::ephemeris
