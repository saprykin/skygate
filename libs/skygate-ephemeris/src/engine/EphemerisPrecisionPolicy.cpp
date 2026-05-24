#include "engine/EphemerisPrecisionPolicy.hpp"

namespace skygate::ephemeris {

EphemerisRequest
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
