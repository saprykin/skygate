#include "EphemerisEngineReplacementPolicy.hpp"
#include "EphemerisEngineFactoryResult.hpp"

namespace skygate::ephemeris {

bool EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
    const EphemerisEngineKind::Type requestedKind,
    const EphemerisEngineFactoryResult& creationResult,
    const EphemerisEngineKind::Type currentEngineKind
) noexcept
{
    return requestedKind == EphemerisEngineKind::Type::HighPrecision && creationResult.usedSimpleEngineFallback()
           && currentEngineKind == EphemerisEngineKind::Type::HighPrecision;
}

}  // namespace skygate::ephemeris
