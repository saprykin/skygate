#include "SimpleEphemerisGuidanceStrategy.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"

#include <memory>
#include <utility>

namespace skygate::ephemeris {

std::unique_ptr<IEphemerisEngine>
SimpleEphemerisGuidanceStrategy::createGuidanceEngine(const CelestialBodyCatalog& catalog) const
{
    return std::move(EphemerisEngineFactory::create(catalog).engine);
}

}  // namespace skygate::ephemeris
