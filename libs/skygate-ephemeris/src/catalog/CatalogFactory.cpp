#include "skygate/ephemeris/CatalogFactory.hpp"

#include "catalog/CatalogBodyNormalization.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <memory>
#include <utility>

namespace skygate::ephemeris {

std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(std::vector<CelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    CatalogBodyNormalization::apply(bodies);
    return std::make_unique<InMemoryStarCatalog>(std::move(bodies));
}

std::unique_ptr<IStarCatalog> createBundledStarCatalog()
{
    CatalogLoadResult result = loadStarCatalog(CatalogSourceType::Bundled);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
