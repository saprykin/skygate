#include "EphemerisCacheGateway.hpp"

#include <utility>

namespace skygate::ephemeris::highprecision {

EphemerisCacheGateway::EphemerisCacheGateway(
    std::shared_ptr<const IEphemerisComputationCache> cache,
    const CelestialBodyCatalog* catalogIdentity,
    EphemerisDatasetInfo dataSetInfo
)
    : m_cache(std::move(cache)), m_catalogIdentity(catalogIdentity), m_dataSetInfo(std::move(dataSetInfo))
{
}

std::optional<CelestialBodyState>
EphemerisCacheGateway::findSnapshotBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    if (m_cache == nullptr) {
        return std::nullopt;
    }

    return m_cache->findSnapshotBodyState(request, m_catalogIdentity, m_dataSetInfo, bodyIndex);
}

}  // namespace skygate::ephemeris::highprecision
