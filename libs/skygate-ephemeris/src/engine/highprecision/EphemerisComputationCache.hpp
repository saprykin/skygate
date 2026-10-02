#pragma once

#include "IEphemerisComputationCache.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

class EphemerisComputationCache final : public IEphemerisComputationCache {
public:
    explicit EphemerisComputationCache(std::size_t maxEntries = 8U);

    [[nodiscard]] std::optional<EphemerisSnapshot> findSnapshot(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    ) const override;

    [[nodiscard]] std::optional<CelestialBodyState> findSnapshotBodyState(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const override;

    void storeSnapshot(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        const EphemerisSnapshot& snapshot
    ) const override;

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    ) const override;

    void storePreparedRequestState(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const override;

    [[nodiscard]] std::optional<CelestialBodyState> findBodyState(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const override;

    void storeBodyState(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex,
        const CelestialBodyState& state
    ) const override;

    void clear() const override;

private:
    struct RequestIdentity {
        EphemerisRequest request;
        const CelestialBodyCatalog* catalogIdentity = nullptr;
        EphemerisDatasetInfo dataSetInfo;
    };

    struct SnapshotEntry {
        RequestIdentity identity;
        EphemerisSnapshot snapshot;
    };

    struct PreparedStateEntry {
        RequestIdentity identity;
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState;
    };

    struct BodyStateEntry {
        RequestIdentity identity;
        std::size_t bodyIndex = 0U;
        CelestialBodyState state;
    };

    [[nodiscard]] static RequestIdentity makeIdentity(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    );
    [[nodiscard]] static bool matchesIdentity(
        const RequestIdentity& identity,
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    );

    [[nodiscard]] static std::string makeRequestKey(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    );

    [[nodiscard]] static std::string makeSnapshotKey(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    );
    [[nodiscard]] static std::string makePreparedStateKey(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo
    );
    [[nodiscard]] static std::string makeBodyStateKey(
        const EphemerisRequest& request,
        const CelestialBodyCatalog* catalogIdentity,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex
    );

    std::size_t m_maxEntries = 8U;
    mutable std::mutex m_mutex;
    mutable std::deque<std::string> m_snapshotOrder;
    mutable std::unordered_map<std::string, SnapshotEntry> m_snapshots;
    mutable std::deque<std::string> m_preparedStateOrder;
    mutable std::unordered_map<std::string, PreparedStateEntry> m_preparedStates;
    mutable std::deque<std::string> m_bodyStateOrder;
    mutable std::unordered_map<std::string, BodyStateEntry> m_bodyStates;
};

}  // namespace skygate::ephemeris::highprecision
