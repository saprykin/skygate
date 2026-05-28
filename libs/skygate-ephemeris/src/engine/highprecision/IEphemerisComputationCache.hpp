#pragma once

#include "EphemerisSnapshot.hpp"
#include "HighPrecisionTypes.hpp"

#include <cstddef>
#include <memory>
#include <optional>

namespace skygate::ephemeris::highprecision {

class IEphemerisComputationCache {
public:
    virtual ~IEphemerisComputationCache() = default;

    [[nodiscard]] virtual std::optional<EphemerisSnapshot> findSnapshot(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo
    ) const = 0;

    virtual void storeSnapshot(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo,
        const EphemerisSnapshot& snapshot
    ) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo
    ) const = 0;

    virtual void storePreparedRequestState(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState> findBodyState(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const = 0;

    virtual void storeBodyState(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> catalogBodies,
        const EphemerisDatasetInfo& dataSetInfo,
        std::size_t bodyIndex,
        const CelestialBodyState& state
    ) const = 0;

    virtual void clear() const = 0;
};

}  // namespace skygate::ephemeris::highprecision
