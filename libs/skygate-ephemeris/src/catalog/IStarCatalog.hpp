#pragma once

#include "Types.hpp"

#include <span>

namespace skygate::ephemeris {

class IStarCatalog {
public:
    virtual ~IStarCatalog() = default;
    [[nodiscard]] virtual std::span<const CelestialBody> bodies() const = 0;
};

}  // namespace skygate::ephemeris
