#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"

#include <span>

namespace skygate::ephemeris {

class IStarCatalog {
public:
    virtual ~IStarCatalog() = default;
    [[nodiscard]] virtual const CelestialBodyCatalog& catalog() const noexcept = 0;
    [[nodiscard]] virtual std::span<const BaseCelestialBody* const> bodies() const = 0;
};

}  // namespace skygate::ephemeris
