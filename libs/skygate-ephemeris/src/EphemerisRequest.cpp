#include "EphemerisRequest.hpp"

#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"

namespace skygate::ephemeris {

EphemerisRequest
EphemerisRequest::fromPrecisionPolicy(EphemerisRequest request, const EphemerisPrecisionPolicy policy) noexcept
{
    if (request.options.engineKind() != EphemerisEngineKind::Type::HighPrecision) {
        return request;
    }

    switch (policy) {
    case EphemerisPrecisionPolicy::SceneRender:
    case EphemerisPrecisionPolicy::Trail: {
        EphemerisCorrectionFlags corrections = EphemerisCorrectionFlags::precessionNutation()
                                               | EphemerisCorrectionFlags::earthOrientation()
                                               | EphemerisCorrectionFlags::diurnalParallax();
        if (request.options.enableAtmosphericRefraction()
            && EphemerisCorrectionFlags::has(
                request.options.correctionFlags(), EphemerisCorrectionFlags::atmosphericRefraction()
            )) {
            corrections |= EphemerisCorrectionFlags::atmosphericRefraction();
        }
        request.options.setCorrectionFlags(corrections);
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
