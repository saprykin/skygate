#pragma once

#include "catalog/composition/CatalogAugmentationResult.hpp"
#include "Types.hpp"

#include <span>

namespace skygate::ephemeris {

class CoreBodyCatalogAugmenter final {
public:
    [[nodiscard]] static CatalogAugmentationResult augment(std::span<const CelestialBody> bodies);
};

}  // namespace skygate::ephemeris
