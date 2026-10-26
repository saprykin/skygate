#include "EphemerisRequest.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"

namespace skygate::ephemeris {

EphemerisRequest EphemerisRequest::fromPrecisionPolicy(
    EphemerisRequest request, const EphemerisPrecisionPolicy policy, const IEphemerisEngine& engine
) noexcept
{
    return EphemerisComputationPolicy::correctionFlagsFor(engine, request, policy);
}

}  // namespace skygate::ephemeris
