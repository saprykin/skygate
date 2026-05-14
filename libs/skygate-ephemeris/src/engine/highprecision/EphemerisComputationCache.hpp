#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris::highprecision {

class EphemerisComputationCache final : public IEphemerisComputationCache {
public:
    explicit EphemerisComputationCache(std::size_t maxEntries = 8U);

    [[nodiscard]] std::optional<SkySnapshot> findSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const override;

    void storeSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        const SkySnapshot& snapshot
    ) const override;

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const override;

    void storePreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const override;

    void clear() const override;

private:
    [[nodiscard]] static std::string makeRequestKey(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    );

    [[nodiscard]] static std::string makeSnapshotKey(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    );
    [[nodiscard]] static std::string makePreparedStateKey(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    );

    std::size_t m_maxEntries = 8U;
    mutable std::mutex m_mutex;
    mutable std::deque<std::string> m_snapshotOrder;
    mutable std::unordered_map<std::string, SkySnapshot> m_snapshots;
    mutable std::deque<std::string> m_preparedStateOrder;
    mutable std::unordered_map<std::string, std::shared_ptr<const PreparedEphemerisRequestState>> m_preparedStates;
};

}  // namespace skygate::ephemeris::highprecision
