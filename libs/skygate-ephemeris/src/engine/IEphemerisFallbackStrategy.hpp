#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"

#include <cstddef>
#include <optional>

namespace skygate::ephemeris {

class IEphemerisFallbackStrategy {
public:
    virtual ~IEphemerisFallbackStrategy() = default;

    [[nodiscard]] virtual std::optional<CelestialBodyState> computeFallbackState(
        const EphemerisRequest& request, const BaseCelestialBody& body, std::size_t bodyIndex
    ) const = 0;
};

}  // namespace skygate::ephemeris
