#include "catalog/CoreBodyCatalogAugmenter.hpp"

#include "catalog/CatalogIdentity.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <algorithm>

namespace skygate::ephemeris {
namespace {

bool isSunOrMoonType(const CelestialBodyType type)
{
    return type == CelestialBodyType::Sun || type == CelestialBodyType::Moon;
}

CatalogCompositionSource sourceKindForBody(const CelestialBody& body)
{
    return catalog_identity::isAnalyticSolarSystemBody(body)
        ? CatalogCompositionSource::BuiltInEphemeris
        : CatalogCompositionSource::Primary;
}

}  // namespace

CatalogAugmentationResult CoreBodyCatalogAugmenter::augment(
    const std::span<const CelestialBody> bodies
)
{
    CatalogAugmentationResult result;
    result.bodies.assign(bodies.begin(), bodies.end());
    result.sourceKinds.reserve(result.bodies.size());
    for (const CelestialBody& body : result.bodies) {
        result.sourceKinds.push_back(sourceKindForBody(body));
    }

    const std::size_t starCount = static_cast<std::size_t>(std::count_if(
        result.bodies.begin(),
        result.bodies.end(),
        [](const CelestialBody& body) {
            return body.type == CelestialBodyType::Star;
        }
    ));

    std::unique_ptr<IStarCatalog> bundledCatalog = createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return result;
    }

    for (const CelestialBody& body : bundledCatalog->bodies()) {
        const bool isReferenceLineStar = body.type == CelestialBodyType::Star
            && catalog_identity::isReferenceLineStarId(body.id)
            && starCount == 0U;
        if (
            !isSunOrMoonType(body.type)
            && body.type != CelestialBodyType::Planet
            && body.type != CelestialBodyType::DeepSkyObject
            && !isReferenceLineStar
        ) {
            continue;
        }

        if (body.type == CelestialBodyType::DeepSkyObject) {
            continue;
        }

        if (catalog_identity::containsBodyId(result.bodies, body.id)) {
            continue;
        }

        result.sourceKinds.push_back(sourceKindForBody(body));
        result.bodies.push_back(body);
    }

    return result;
}

}  // namespace skygate::ephemeris
