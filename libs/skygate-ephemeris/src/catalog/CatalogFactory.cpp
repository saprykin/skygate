#include "CatalogFactory.hpp"
#include "CatalogLoader.hpp"
#include "catalog/InMemoryStarCatalog.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

#include <QtGlobal>

#include <memory>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

std::unique_ptr<IStarCatalog> CatalogFactory::createStarCatalogFromBodies(std::vector<OwnGalaxyCelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    const CatalogSnapshotValidator::Report report = CatalogSnapshotValidator::validate(bodies);
    Q_ASSERT_X(report.ok, "CatalogFactory", report.errorDetail.c_str());
    if (!report.ok) {
        return nullptr;
    }

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

    const CatalogSnapshotValidator::Report report =
        CatalogSnapshotValidator::validate(ownGalaxyBodies, distantBodies, orderedBodyIndexes);
    Q_ASSERT_X(report.ok, "CatalogFactory", report.errorDetail.c_str());
    if (!report.ok) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(
        CelestialBodyCatalog(std::move(ownGalaxyBodies), std::move(distantBodies), std::move(orderedBodyIndexes))
    );
}

std::unique_ptr<IStarCatalog> CatalogFactory::createStarCatalogFromCatalog(CelestialBodyCatalog catalog)
{
    if (catalog.empty()) {
        return nullptr;
    }

    const auto ownGalaxyBodies = catalog.ownGalaxyBodies();
    const auto distantBodies = catalog.distantBodies();
    const auto orderedIndexes = catalog.orderedBodyIndexes();

    return createStarCatalogFromBodies(
        std::vector<OwnGalaxyCelestialBody>(ownGalaxyBodies.begin(), ownGalaxyBodies.end()),
        std::vector<DistantCelestialBody>(distantBodies.begin(), distantBodies.end()),
        std::vector<CelestialBodyCatalog::OrderEntry>(orderedIndexes.begin(), orderedIndexes.end())
    );
}

std::unique_ptr<IStarCatalog> CatalogFactory::createBundledStarCatalog()
{
    CatalogLoadResult result = CatalogLoader::load(CatalogSourceType::Bundled);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
