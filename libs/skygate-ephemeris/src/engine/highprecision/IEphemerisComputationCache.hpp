#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <QtGlobal>

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
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        return std::nullopt;
    }

    virtual void storeSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        const SkySnapshot& snapshot
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        Q_UNUSED(snapshot);
    }

    [[nodiscard]] virtual std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        return nullptr;
    }

    virtual void storePreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        Q_UNUSED(preparedState);
    }

    [[nodiscard]] virtual std::optional<CelestialBodyState> findBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        Q_UNUSED(bodyIndex);
        return std::nullopt;
    }

    virtual void storeBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex,
        const CelestialBodyState& state
    ) const
    {
        Q_UNUSED(request);
        Q_UNUSED(catalogBodies);
        Q_UNUSED(dataSetInfo);
        Q_UNUSED(bodyIndex);
        Q_UNUSED(state);
    }

    virtual void clear() const {}
};

}  // namespace skygate::ephemeris::highprecision
