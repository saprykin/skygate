#pragma once

#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogCompositionSource.hpp"

#include <vector>

namespace skygate::ephemeris {

struct CatalogAugmentationResult final {
    std::vector<OwnGalaxyCelestialBody> bodies;
    std::vector<CatalogCompositionSource> sourceKinds;
};

}  // namespace skygate::ephemeris
