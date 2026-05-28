#pragma once

#include "OwnGalaxyCelestialBody.hpp"

#include <vector>

namespace skygate::ephemeris {

class CatalogBodyNormalization final {
public:
    static void apply(OwnGalaxyCelestialBody& body);
    static void apply(std::vector<OwnGalaxyCelestialBody>& bodies);
};

}  // namespace skygate::ephemeris
