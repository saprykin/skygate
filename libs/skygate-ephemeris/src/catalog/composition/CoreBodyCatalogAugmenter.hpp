#pragma once

#include "BaseCelestialBody.hpp"
#include "CatalogAugmentationResult.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <span>
#include <vector>

namespace skygate::ephemeris {

class CoreBodyCatalogAugmenter final {
public:
    [[nodiscard]] static CatalogAugmentationResult augment(std::span<const BaseCelestialBody* const> bodies);

    // The bundled bright-star fallback bodies used when a composition has no
    // other star source.
    [[nodiscard]] static std::vector<OwnGalaxyCelestialBody> bundledBrightStars();
};

}  // namespace skygate::ephemeris
