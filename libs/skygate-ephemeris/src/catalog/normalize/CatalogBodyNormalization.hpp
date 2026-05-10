#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <vector>

namespace skygate::ephemeris {

class CatalogBodyNormalization final {
public:
    static void apply(CelestialBody& body);
    static void apply(std::vector<CelestialBody>& bodies);
};

}  // namespace skygate::ephemeris
