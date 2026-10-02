#pragma once

#include "engine/IEphemerisFallbackStrategy.hpp"

#include <cstddef>
#include <optional>

namespace skygate::ephemeris {

class SimpleEphemerisFallbackStrategy final : public IEphemerisFallbackStrategy {
public:
    [[nodiscard]] std::optional<CelestialBodyState> computeFallbackState(
        const EphemerisRequest& request, const BaseCelestialBody& body, std::size_t bodyIndex
    ) const override;
};

}  // namespace skygate::ephemeris
