#include "CatalogComposer.hpp"
#include "CatalogFactory.hpp"
#include "CatalogIdentity.hpp"
#include "catalog/composition/CoreBodyCatalogAugmenter.hpp"
#include "catalog/composition/DeepSkyCatalogMerger.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

void assignCompositionCounts(
    ActiveCatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::size_t currentConstellationCount
)
{
    std::size_t catalogConstellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    for (const BaseCelestialBody* body : bodies) {
        if (body == nullptr) {
            continue;
        }

        if (body->kind == BaseCelestialBody::Kind::Constellation) {
            ++catalogConstellationCount;
        }
        if (body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            ++deepSkyObjectCount;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(catalogConstellationCount, currentConstellationCount);
    result.deepSkyObjectCount = deepSkyObjectCount;
}

DistantCelestialBody toDistantBody(const BaseCelestialBody& body)
{
    DistantCelestialBody distantBody;
    distantBody.id = body.id;
    distantBody.displayName = body.displayName;
    distantBody.kind = body.kind;
    distantBody.visualMagnitude = body.visualMagnitude;
    distantBody.fixedEquatorial = body.fixedEquatorialValue();
    distantBody.deepSkyObject = body.deepSkyObjectValue();
    return distantBody;
}

std::vector<DistantCelestialBody>
collectPrimaryDeepSkyBodies(const std::span<const BaseCelestialBody* const> sourceBodies)
{
    std::vector<DistantCelestialBody> bodies;
    for (const BaseCelestialBody* body : sourceBodies) {
        if (body != nullptr && body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            bodies.push_back(toDistantBody(*body));
        }
    }
    return bodies;
}

std::vector<CelestialBodyCatalog::OrderEntry>
buildAugmentedOrder(const std::size_t ownGalaxyBodyCount, const std::size_t distantBodyCount)
{
    std::vector<CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyBodyCount + distantBodyCount);
    for (std::size_t index = 0U; index < ownGalaxyBodyCount; ++index) {
        order.push_back({.domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = index});
    }
    for (std::size_t index = 0U; index < distantBodyCount; ++index) {
        order.push_back({.domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = index});
    }
    return order;
}

}  // namespace

ActiveCatalogCompositionResult CatalogComposer::compose(const ActiveCatalogCompositionRequest& request)
{
    ActiveCatalogCompositionResult result;
    const IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    std::unique_ptr<IStarCatalog> bundledDeepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        bundledDeepSkyCatalog = CatalogFactory::createBundledStarCatalog();
        deepSkyCatalog = bundledDeepSkyCatalog.get();
    }

    CatalogAugmentationResult active = CoreBodyCatalogAugmenter::augment(request.sourceCatalog.bodies());
    std::vector<DistantCelestialBody> primaryDeepSkyBodies =
        collectPrimaryDeepSkyBodies(request.sourceCatalog.bodies());
    std::vector<CelestialBodyCatalog::OrderEntry> augmentedOrder =
        buildAugmentedOrder(active.bodies.size(), primaryDeepSkyBodies.size());
    active.sourceKinds.reserve(active.sourceKinds.size() + primaryDeepSkyBodies.size());
    for (std::size_t index = 0U; index < primaryDeepSkyBodies.size(); ++index) {
        active.sourceKinds.push_back(CatalogCompositionSource::Primary);
    }

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (deepSkyCatalog != nullptr) {
        if (result.foundDeepSkyObjectCount == 0U || request.useBundledDeepSkyCatalog) {
            result.foundDeepSkyObjectCount = CatalogIdentity::countDeepSkyObjects(deepSkyCatalog->bodies());
        }

        const CelestialBodyCatalog activeCatalog(active.bodies, primaryDeepSkyBodies, augmentedOrder);
        DeepSkyCatalogMergeResult merged =
            DeepSkyCatalogMerger::merge(activeCatalog.bodies(), active.sourceKinds, deepSkyCatalog->bodies());
        result.catalog = CatalogFactory::createStarCatalogFromBodies(
            std::move(merged.ownGalaxyBodies), std::move(merged.distantBodies), std::move(merged.orderedBodyIndexes)
        );
        active.sourceKinds = std::move(merged.sourceKinds);
    }

    if (result.catalog == nullptr) {
        result.catalog = CatalogFactory::createStarCatalogFromBodies(
            std::move(active.bodies), std::move(primaryDeepSkyBodies), std::move(augmentedOrder)
        );
    }
    if (result.catalog == nullptr) {
        result.sourceKinds.clear();
        return result;
    }

    result.sourceKinds = std::move(active.sourceKinds);
    assignCompositionCounts(result, result.catalog->bodies(), request.currentConstellationCount);
    return result;
}

}  // namespace skygate::ephemeris
