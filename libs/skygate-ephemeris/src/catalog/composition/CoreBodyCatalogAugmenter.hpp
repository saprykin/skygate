#pragma once

#include "skygate/ephemeris/CatalogComposer.hpp"

#include <span>
#include <vector>

namespace skygate::ephemeris {

struct CatalogAugmentationResult final {
    std::vector<CelestialBody> bodies;
    std::vector<CatalogCompositionSource> sourceKinds;
};

class CoreBodyCatalogAugmenter final {
public:
    [[nodiscard]] static CatalogAugmentationResult augment(std::span<const CelestialBody> bodies);
};

}  // namespace skygate::ephemeris
