#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace skygate::ephemeris::highprecision {

class IEphemerisComputationCache {
public:
    virtual ~IEphemerisComputationCache() = default;

    [[nodiscard]] virtual std::optional<SkySnapshot> findSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const = 0;

    virtual void storeSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        const SkySnapshot& snapshot
    ) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const = 0;

    virtual void storePreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState> findBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const = 0;

    virtual void storeBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex,
        const CelestialBodyState& state
    ) const = 0;

    virtual void clear() const = 0;
};

}  // namespace skygate::ephemeris::highprecision
