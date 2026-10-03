#include "EphemerisEngineReplacementPolicy.hpp"
#include "EphemerisEngineFactoryResult.hpp"

namespace skygate::ephemeris {

bool EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
    const EphemerisEngineKind::Type requestedKind,
    const EphemerisEngineFactoryResult& creationResult,
    const EphemerisEngineKind::Type currentEngineKind
) noexcept
{
    return creationResult.usedSimpleEngineFallback() && currentEngineKind == requestedKind;
}

}  // namespace skygate::ephemeris
