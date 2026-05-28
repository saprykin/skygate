#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "ObservationContext.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisSnapshot {
    core::ObservationContext context;
    std::shared_ptr<const CelestialBodyCatalog> catalogBodies;
    std::vector<CelestialBodyState> states;

    [[nodiscard]] std::span<const BaseCelestialBody* const> bodies() const noexcept;
    [[nodiscard]] const BaseCelestialBody& bodyAt(std::size_t bodyIndex) const noexcept;
};

}  // namespace skygate::ephemeris
