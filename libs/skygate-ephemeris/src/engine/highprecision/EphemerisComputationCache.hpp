#pragma once

#include "IEphemerisComputationCache.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

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
    // Keyed store bounded by capacity. Entries are evicted in insertion order;
    // a lookup counts as a hit but does not refresh that order.
    template <typename Entry>
    class KeyedLruCache final {
    public:
        explicit KeyedLruCache(const std::size_t maxEntries) noexcept : m_maxEntries(maxEntries == 0U ? 1U : maxEntries)
        {
        }

        // The owning cache serializes access with its own mutex.
        [[nodiscard]] const Entry* find(const std::string& key) const noexcept
        {
            const auto entry = m_entries.find(key);
            return entry == m_entries.end() ? nullptr : &entry->second;
        }

        void store(const std::string& key, Entry entry)
        {
            if (!m_entries.contains(key)) {
                m_order.push_back(key);
            }
            m_entries.insert_or_assign(key, std::move(entry));

            while (m_order.size() > m_maxEntries) {
                m_entries.erase(m_order.front());
                m_order.pop_front();
            }
        }

        void clear() noexcept
        {
            m_order.clear();
            m_entries.clear();
        }

    private:
        std::size_t m_maxEntries = 1U;
        std::deque<std::string> m_order;
        std::unordered_map<std::string, Entry> m_entries;
    };

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

    mutable std::mutex m_mutex;
    mutable KeyedLruCache<SnapshotEntry> m_snapshots;
    mutable KeyedLruCache<PreparedStateEntry> m_preparedStates;
    mutable KeyedLruCache<BodyStateEntry> m_bodyStates;
};

}  // namespace skygate::ephemeris::highprecision
