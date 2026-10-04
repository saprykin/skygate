#include "CatalogComposer.hpp"

#include "CatalogCompositionMerger.hpp"
#include "CatalogFactory.hpp"
#include "CatalogIdentity.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

void assignCompositionCounts(
    CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::size_t currentConstellationCount
)
{
    std::size_t catalogConstellationCount = 0;
    for (const BaseCelestialBody* body : bodies) {
        if (body != nullptr && body->kind == BaseCelestialBody::Kind::Constellation) {
            ++catalogConstellationCount;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(catalogConstellationCount, currentConstellationCount);
    result.deepSkyObjectCount = CatalogIdentity::countDeepSkyObjects(bodies);
}

std::size_t countDeepSkyObjects(const IStarCatalog* catalog)
{
    return catalog != nullptr ? CatalogIdentity::countDeepSkyObjects(catalog->bodies()) : 0U;
}

}  // namespace

CatalogCompositionResult CatalogComposer::composeCollection(const CatalogCompositionRequest& request)
{
    CatalogCompositionResult result;
    CatalogCompositionMergeResult merged = CatalogCompositionMerger::mergeCollection(request);
    result.catalog = CatalogFactory::createStarCatalogFromBodies(
        std::move(merged.ownGalaxyBodies), std::move(merged.distantBodies), std::move(merged.orderedBodyIndexes)
    );
    if (result.catalog == nullptr) {
        result.sourceIds.clear();
        return result;
    }

    result.sourceIds = std::move(merged.sourceIds);
    assignCompositionCounts(result, result.catalog->bodies(), request.currentConstellationCount);

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (result.foundDeepSkyObjectCount == 0U) {
        for (const CatalogCompositionSourceEntry& source : request.sources) {
            if (!source.enabled || source.catalog == nullptr) {
                continue;
            }
            if (source.policy == CatalogCompositionPolicy::DeepSkyOnly) {
                result.foundDeepSkyObjectCount += countDeepSkyObjects(source.catalog);
            }
        }
    }
    return result;
}

ActiveCatalogCompositionResult CatalogComposer::compose(const ActiveCatalogCompositionRequest& request)
{
    std::unique_ptr<IStarCatalog> bundledCatalog = CatalogFactory::createBundledStarCatalog();
    const IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        deepSkyCatalog = bundledCatalog.get();
    }

    CatalogCompositionRequest collectionRequest;
    collectionRequest.currentConstellationCount = request.currentConstellationCount;
    collectionRequest.knownDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    collectionRequest.sources.push_back(
        CatalogCompositionSourceEntry{
            .sourceId = std::string(CatalogCompositionMerger::sourceKindId(CatalogCompositionSource::Primary)),
            .enabled = true,
            .catalog = &request.sourceCatalog,
            .policy = CatalogCompositionPolicy::Merge,
        }
    );
    collectionRequest.sources.push_back(
        CatalogCompositionSourceEntry{
            .sourceId = std::string(CatalogCompositionMerger::sourceKindId(CatalogCompositionSource::BuiltInEphemeris)),
            .enabled = true,
            .catalog = bundledCatalog.get(),
            .policy = CatalogCompositionPolicy::AugmentCore,
        }
    );
    if (deepSkyCatalog != nullptr) {
        collectionRequest.sources.push_back(
            CatalogCompositionSourceEntry{
                .sourceId = std::string(CatalogCompositionMerger::sourceKindId(CatalogCompositionSource::DeepSky)),
                .enabled = true,
                .catalog = deepSkyCatalog,
                .policy = CatalogCompositionPolicy::DeepSkyOnly,
            }
        );
    }

    CatalogCompositionResult composed = composeCollection(collectionRequest);
    ActiveCatalogCompositionResult result;
    result.catalog = std::move(composed.catalog);
    if (result.catalog == nullptr) {
        result.sourceKinds.clear();
        return result;
    }

    result.bodyCount = composed.bodyCount;
    result.constellationCount = composed.constellationCount;
    result.deepSkyObjectCount = composed.deepSkyObjectCount;
    result.foundDeepSkyObjectCount = composed.foundDeepSkyObjectCount;
    if (deepSkyCatalog != nullptr && (request.knownDeepSkyObjectCount == 0U || request.useBundledDeepSkyCatalog)) {
        result.foundDeepSkyObjectCount = CatalogIdentity::countDeepSkyObjects(deepSkyCatalog->bodies());
    }

    result.sourceKinds.reserve(composed.sourceIds.size());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    for (std::size_t index = 0; index < composed.sourceIds.size(); ++index) {
        const BaseCelestialBody* body = index < bodies.size() ? bodies[index] : nullptr;
        if (body != nullptr && CatalogIdentity::isAnalyticSolarSystemBody(*body)) {
            result.sourceKinds.push_back(CatalogCompositionSource::BuiltInEphemeris);
            continue;
        }
        result.sourceKinds.push_back(CatalogCompositionMerger::sourceKindFromId(composed.sourceIds[index]));
    }
    return result;
}

}  // namespace skygate::ephemeris
