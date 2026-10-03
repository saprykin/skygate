#pragma once

#include "IEarthOrientationProvider.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "ITimeScaleService.hpp"

#include <memory>

namespace skygate::ephemeris {

/// Composes Earth-orientation and time-scale providers from an ephemeris data
/// snapshot. Shared by the engine factory and application-level callers that
/// cache providers across engine rebuilds.
class TimeScaleProviderLoader final {
public:
    [[nodiscard]] static std::shared_ptr<const IEarthOrientationProvider>
    loadEarthOrientationProvider(const IEphemerisDataSnapshot& snapshot);

    [[nodiscard]] static std::shared_ptr<const ITimeScaleService> loadTimeScaleService(
        const IEphemerisDataSnapshot& snapshot,
        const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider
    );
};

}  // namespace skygate::ephemeris
