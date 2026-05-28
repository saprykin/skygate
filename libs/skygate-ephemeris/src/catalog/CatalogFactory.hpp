#pragma once

#include "IStarCatalog.hpp"

#include <memory>
#include <vector>

namespace skygate::ephemeris {

class CatalogFactory final {
public:
    [[nodiscard]] static std::unique_ptr<IStarCatalog>
    createStarCatalogFromBodies(std::vector<OwnGalaxyCelestialBody> bodies);
    [[nodiscard]] static std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(
        std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies,
        std::vector<DistantCelestialBody> distantBodies,
        std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes
    );
    [[nodiscard]] static std::unique_ptr<IStarCatalog> createStarCatalogFromCatalog(CelestialBodyCatalog catalog);
    [[nodiscard]] static std::unique_ptr<IStarCatalog> createBundledStarCatalog();
};

}  // namespace skygate::ephemeris
