#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <vector>

namespace skygate::ephemeris {

class IStarCatalog {
public:
    virtual ~IStarCatalog() = default;
    [[nodiscard]] virtual std::vector<CelestialBody> bodies() const = 0;
};

}  // namespace skygate::ephemeris
