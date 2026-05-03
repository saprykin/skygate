#include "catalog/DeepSkyCatalogMerger.hpp"

#include "catalog/CatalogIdentity.hpp"

#include <algorithm>

namespace skygate::ephemeris {

DeepSkyCatalogMergeResult DeepSkyCatalogMerger::merge(
    const std::span<const CelestialBody> activeBodies,
    const std::span<const CatalogCompositionSource> activeSourceKinds,
    const std::span<const CelestialBody> deepSkyBodies
)
{
    DeepSkyCatalogMergeResult result;
    result.bodies.reserve(activeBodies.size() + deepSkyBodies.size());
    result.sourceKinds.reserve(activeBodies.size() + deepSkyBodies.size());

    for (std::size_t index = 0; index < activeBodies.size(); ++index) {
        const CelestialBody& body = activeBodies[index];
        if (body.type == CelestialBodyType::DeepSkyObject) {
            continue;
        }

        result.bodies.push_back(body);
        result.sourceKinds.push_back(activeSourceKinds[index]);
    }

    for (std::size_t index = 0; index < activeBodies.size(); ++index) {
        const CelestialBody& body = activeBodies[index];
        if (body.type != CelestialBodyType::DeepSkyObject) {
            continue;
        }

        const bool replacedByDeepSkyObject = std::any_of(
            deepSkyBodies.begin(),
            deepSkyBodies.end(),
            [&body](const CelestialBody& deepSkyBody) {
                return deepSkyBody.type == CelestialBodyType::DeepSkyObject
                    && catalog_identity::sharesDeepSkyAlias(body, deepSkyBody);
            }
        );
        if (replacedByDeepSkyObject) {
            continue;
        }

        result.bodies.push_back(body);
        result.sourceKinds.push_back(activeSourceKinds[index]);
    }

    for (const CelestialBody& body : deepSkyBodies) {
        if (body.type != CelestialBodyType::DeepSkyObject) {
            continue;
        }

        result.bodies.push_back(body);
        result.sourceKinds.push_back(CatalogCompositionSource::DeepSky);
    }

    return result;
}

}  // namespace skygate::ephemeris
