#pragma once

#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogCompositionSource.hpp"

#include <vector>

namespace skygate::ephemeris {

struct DeepSkyCatalogMergeResult final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    std::vector<CatalogCompositionSource> sourceKinds;
};

}  // namespace skygate::ephemeris
