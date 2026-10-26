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
// per surviving output body. contributorSourceIds is parallel to sourceIds and
// lists every source that contributed to the surviving body in precedence
// order, so merged objects retain the identity of each contributor rather than
// only the winning source.
struct CatalogCompositionMergeResult final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    std::vector<std::string> sourceIds;
    std::vector<std::vector<std::string>> contributorSourceIds;
};

}  // namespace skygate::ephemeris
