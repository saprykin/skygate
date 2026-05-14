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
    explicit EphemerisComputationCache(std::size_t maxSnapshotEntries = 8U);

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

    void clear() const override;

private:
    [[nodiscard]] static std::string makeSnapshotKey(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    );

    std::size_t m_maxSnapshotEntries = 8U;
    mutable std::mutex m_mutex;
    mutable std::deque<std::string> m_snapshotOrder;
    mutable std::unordered_map<std::string, SkySnapshot> m_snapshots;
};

}  // namespace skygate::ephemeris::highprecision
