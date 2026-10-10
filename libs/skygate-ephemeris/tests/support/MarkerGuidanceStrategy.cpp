#include "MarkerGuidanceStrategy.hpp"

#include <memory>

namespace skygate::ephemeris::tests {

std::unique_ptr<IEphemerisEngine> MarkerGuidanceStrategy::createGuidanceEngine(const CelestialBodyCatalog&) const
{
    ++createCount;
    return std::make_unique<MarkerGuidanceEngine>(m_sampleCount);
}

}  // namespace skygate::ephemeris::tests
