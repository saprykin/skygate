#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <vector>

namespace skygate::ephemeris {

class CatalogFactory final {
public:
    [[nodiscard]] static std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(
        std::vector<CelestialBody> bodies
    );
    [[nodiscard]] static std::unique_ptr<IStarCatalog> createBundledStarCatalog();
};

}  // namespace skygate::ephemeris
