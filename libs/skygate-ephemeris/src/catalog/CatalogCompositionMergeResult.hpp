#pragma once

#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

// Raw merge output before snapshot construction.
//
// sourceIds is parallel to orderedBodyIndexes: one stable source instance ID
// per surviving output body.
struct CatalogCompositionMergeResult final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    std::vector<std::string> sourceIds;
};

}  // namespace skygate::ephemeris
