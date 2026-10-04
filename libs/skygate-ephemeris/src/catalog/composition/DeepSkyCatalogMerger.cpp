#include "DeepSkyCatalogMerger.hpp"
#include "catalog/CatalogIdentity.hpp"

#include <algorithm>

namespace skygate::ephemeris {
namespace {

void pushBody(
    DeepSkyCatalogMergeResult& result, const BaseCelestialBody& body, const CatalogCompositionSource sourceKind
)
{
    if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        result.orderedBodyIndexes.push_back(
            CelestialBodyCatalog::OrderEntry{
                .domain = CelestialBodyCatalog::BodyDomain::Distant,
                .bodyIndex = result.distantBodies.size(),
            }
        );
        result.distantBodies.push_back(CelestialBodyCatalog::copyDistantBody(body));
    } else {
        result.orderedBodyIndexes.push_back(
            CelestialBodyCatalog::OrderEntry{
                .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = result.ownGalaxyBodies.size(),
            }
        );
        result.ownGalaxyBodies.push_back(CelestialBodyCatalog::copyOwnGalaxyBody(body));
    }

    result.sourceKinds.push_back(sourceKind);
}

}  // namespace

DeepSkyCatalogMergeResult DeepSkyCatalogMerger::merge(
    const std::span<const BaseCelestialBody* const> activeBodies,
    const std::span<const CatalogCompositionSource> activeSourceKinds,
    const std::span<const BaseCelestialBody* const> deepSkyBodies
)
{
    DeepSkyCatalogMergeResult result;
    result.ownGalaxyBodies.reserve(activeBodies.size());
    result.distantBodies.reserve(deepSkyBodies.size());
    result.orderedBodyIndexes.reserve(activeBodies.size() + deepSkyBodies.size());
    result.sourceKinds.reserve(activeBodies.size() + deepSkyBodies.size());

    for (std::size_t index = 0; index < activeBodies.size(); ++index) {
        const BaseCelestialBody& body = *activeBodies[index];
        if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }

        pushBody(result, body, activeSourceKinds[index]);
    }

    for (std::size_t index = 0; index < activeBodies.size(); ++index) {
        const BaseCelestialBody& body = *activeBodies[index];
        if (body.kind != BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }

        const bool replacedByDeepSkyObject =
            std::any_of(deepSkyBodies.begin(), deepSkyBodies.end(), [&body](const BaseCelestialBody* deepSkyBody) {
                return deepSkyBody != nullptr && deepSkyBody->kind == BaseCelestialBody::Kind::DeepSkyObject
                       && CatalogIdentity::sharesDeepSkyAlias(body, *deepSkyBody);
            });
        if (replacedByDeepSkyObject) {
            continue;
        }

        pushBody(result, body, activeSourceKinds[index]);
    }

    for (const BaseCelestialBody* body : deepSkyBodies) {
        if (body == nullptr || body->kind != BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }

        pushBody(result, *body, CatalogCompositionSource::DeepSky);
    }

    return result;
}

}  // namespace skygate::ephemeris
