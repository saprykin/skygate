#include "skygate/ephemeris/CatalogComposer.hpp"

#include "catalog/CatalogIdentity.hpp"
#include "catalog/CoreBodyCatalogAugmenter.hpp"
#include "catalog/DeepSkyCatalogMerger.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace skygate::ephemeris {
namespace {

void assignCompositionCounts(
    ActiveCatalogCompositionResult& result,
    const std::span<const CelestialBody> bodies,
    const std::size_t currentConstellationCount
)
{
    std::size_t catalogConstellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    for (const auto& body : bodies) {
        if (body.type == CelestialBodyType::Constellation) {
            ++catalogConstellationCount;
        }
        if (body.type == CelestialBodyType::DeepSkyObject) {
            ++deepSkyObjectCount;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(catalogConstellationCount, currentConstellationCount);
    result.deepSkyObjectCount = deepSkyObjectCount;
}

}  // namespace

bool ActiveCatalogCompositionResult::isSuccess() const noexcept
{
    return catalog != nullptr;
}

ActiveCatalogCompositionResult composeActiveCatalog(
    const ActiveCatalogCompositionRequest& request
)
{
    ActiveCatalogCompositionResult result;
    const IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    std::unique_ptr<IStarCatalog> bundledDeepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        bundledDeepSkyCatalog = createBundledStarCatalog();
        deepSkyCatalog = bundledDeepSkyCatalog.get();
    }

    CatalogAugmentationResult active = CoreBodyCatalogAugmenter::augment(request.sourceCatalog.bodies());

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (deepSkyCatalog != nullptr) {
        if (result.foundDeepSkyObjectCount == 0U || request.useBundledDeepSkyCatalog) {
            result.foundDeepSkyObjectCount = catalog_identity::countDeepSkyObjects(deepSkyCatalog->bodies());
        }

        DeepSkyCatalogMergeResult merged = DeepSkyCatalogMerger::merge(
            active.bodies,
            active.sourceKinds,
            deepSkyCatalog->bodies()
        );
        active.bodies = std::move(merged.bodies);
        active.sourceKinds = std::move(merged.sourceKinds);
    }

    result.catalog = createStarCatalogFromBodies(std::move(active.bodies));
    if (result.catalog == nullptr) {
        result.sourceKinds.clear();
        return result;
    }

    result.sourceKinds = std::move(active.sourceKinds);
    assignCompositionCounts(result, result.catalog->bodies(), request.currentConstellationCount);
    return result;
}

}  // namespace skygate::ephemeris
