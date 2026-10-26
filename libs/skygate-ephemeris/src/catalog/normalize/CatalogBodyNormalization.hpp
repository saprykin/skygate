#pragma once

#include "OwnGalaxyCelestialBody.hpp"

namespace skygate::ephemeris {

class CatalogBodyNormalization final {
public:
    static void apply(OwnGalaxyCelestialBody& body);
};

}  // namespace skygate::ephemeris
