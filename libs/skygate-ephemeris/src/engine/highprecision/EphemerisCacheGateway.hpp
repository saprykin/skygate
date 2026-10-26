#pragma once

#include "IEphemerisComputationCache.hpp"

#include <cstddef>
#include <memory>
#include <optional>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

// Read-through front end for the computation cache. The request identity that
// every cache call needs, the catalog identity and the data set description, is
// fixed at construction, so callers hand over only the request and the callback
// that produces the value on a miss. Without a cache the callbacks run directly.
class EphemerisCacheGateway final {
public:
    EphemerisCacheGateway(
        std::shared_ptr<const IEphemerisComputationCache> cache,
        const CelestialBodyCatalog* catalogIdentity,
        EphemerisDatasetInfo dataSetInfo
    );

    [[nodiscard]] std::optional<CelestialBodyState>
    findSnapshotBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const;

    template <typename ComputeSnapshot>
    [[nodiscard]] EphemerisSnapshot snapshot(const EphemerisRequest& request, ComputeSnapshot compute) const
    {
        if (m_cache != nullptr) {
            if (std::optional<EphemerisSnapshot> cachedSnapshot =
                    m_cache->findSnapshot(request, m_catalogIdentity, m_dataSetInfo);
                cachedSnapshot.has_value()) {
                return *cachedSnapshot;
            }
        }

        EphemerisSnapshot computedSnapshot = compute();
        if (m_cache != nullptr) {
            m_cache->storeSnapshot(request, m_catalogIdentity, m_dataSetInfo, computedSnapshot);
        }
        return computedSnapshot;
    }

    template <typename ComputeBodyState>
    [[nodiscard]] CelestialBodyState
    bodyState(const EphemerisRequest& request, std::size_t bodyIndex, ComputeBodyState compute) const
    {
        if (m_cache != nullptr) {
            if (std::optional<CelestialBodyState> cachedBodyState =
                    m_cache->findBodyState(request, m_catalogIdentity, m_dataSetInfo, bodyIndex);
                cachedBodyState.has_value()) {
                return *cachedBodyState;
            }
        }

        CelestialBodyState computedBodyState = compute();
        if (m_cache != nullptr) {
            m_cache->storeBodyState(request, m_catalogIdentity, m_dataSetInfo, bodyIndex, computedBodyState);
        }
        return computedBodyState;
    }

    template <typename BuildPreparedState>
    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    preparedRequestState(const EphemerisRequest& request, BuildPreparedState build) const
    {
        if (m_cache != nullptr) {
            if (std::shared_ptr<const PreparedEphemerisRequestState> cachedPreparedState =
                    m_cache->findPreparedRequestState(request, m_catalogIdentity, m_dataSetInfo);
                cachedPreparedState != nullptr) {
                return cachedPreparedState;
            }
        }

        std::shared_ptr<const PreparedEphemerisRequestState> preparedState = build();
        if (m_cache != nullptr) {
            m_cache->storePreparedRequestState(request, m_catalogIdentity, m_dataSetInfo, preparedState);
        }
        return preparedState;
    }

private:
    std::shared_ptr<const IEphemerisComputationCache> m_cache;
    const CelestialBodyCatalog* m_catalogIdentity = nullptr;
    EphemerisDatasetInfo m_dataSetInfo;
};

}  // namespace skygate::ephemeris::highprecision
