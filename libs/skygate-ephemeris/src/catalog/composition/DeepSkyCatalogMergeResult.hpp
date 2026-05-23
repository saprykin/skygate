#pragma once

#include "Types.hpp"
#include "catalog/CatalogCompositionSource.hpp"

#include <vector>

namespace skygate::ephemeris {

struct DeepSkyCatalogMergeResult final {
    std::vector<CelestialBody> bodies;
    std::vector<CatalogCompositionSource> sourceKinds;
};

}  // namespace skygate::ephemeris
