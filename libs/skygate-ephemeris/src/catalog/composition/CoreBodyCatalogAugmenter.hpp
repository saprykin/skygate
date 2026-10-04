#pragma once

#include "BaseCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <vector>

namespace skygate::ephemeris {

class CoreBodyCatalogAugmenter final {
public:
    // The bundled bright-star fallback bodies used when a composition has no
    // other star source.
    [[nodiscard]] static std::vector<OwnGalaxyCelestialBody> bundledBrightStars();
};

}  // namespace skygate::ephemeris
