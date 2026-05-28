#include "CatalogFactory.hpp"
#include "CatalogLoader.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "catalog/normalize/CatalogBodyNormalization.hpp"

#include <memory>
#include <utility>

namespace skygate::ephemeris {

std::unique_ptr<IStarCatalog> CatalogFactory::createStarCatalogFromBodies(std::vector<OwnGalaxyCelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    CatalogBodyNormalization::apply(bodies);
    return std::make_unique<InMemoryStarCatalog>(CelestialBodyCatalog(std::move(bodies)));
}

std::unique_ptr<IStarCatalog> CatalogFactory::createStarCatalogFromBodies(
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<DistantCelestialBody> distantBodies,
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes
)
{
    if (ownGalaxyBodies.empty() && distantBodies.empty()) {
        return nullptr;
    }

    CatalogBodyNormalization::apply(ownGalaxyBodies);
    return std::make_unique<InMemoryStarCatalog>(
        CelestialBodyCatalog(std::move(ownGalaxyBodies), std::move(distantBodies), std::move(orderedBodyIndexes))
    );
}

std::unique_ptr<IStarCatalog> CatalogFactory::createStarCatalogFromCatalog(CelestialBodyCatalog catalog)
{
    if (catalog.empty()) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(std::move(catalog));
}

std::unique_ptr<IStarCatalog> CatalogFactory::createBundledStarCatalog()
{
    CatalogLoadResult result = CatalogLoader::load(CatalogSourceType::Bundled);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
