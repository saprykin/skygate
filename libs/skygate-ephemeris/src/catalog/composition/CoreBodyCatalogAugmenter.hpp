#pragma once

#include "BaseCelestialBody.hpp"
#include "CatalogAugmentationResult.hpp"

#include <span>

namespace skygate::ephemeris {

class CoreBodyCatalogAugmenter final {
public:
    [[nodiscard]] static CatalogAugmentationResult augment(std::span<const BaseCelestialBody* const> bodies);
};

}  // namespace skygate::ephemeris
