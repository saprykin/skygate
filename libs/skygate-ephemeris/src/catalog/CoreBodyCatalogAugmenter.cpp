#include "catalog/CoreBodyCatalogAugmenter.hpp"

#include "catalog/CatalogIdentity.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

struct ReferenceLineStar final {
    std::string_view id;
    std::string_view displayName;
    double rightAscensionHours;
    double declinationDeg;
    double visualMagnitude;
};

constexpr std::array<ReferenceLineStar, 8> kReferenceLineStars {{
    {"sirius", "Sirius", 6.7525, -16.7161, -1.46},
    {"canopus", "Canopus", 6.3992, -52.6957, -0.74},
    {"arcturus", "Arcturus", 14.2610, 19.1825, -0.05},
    {"vega", "Vega", 18.6156, 38.7837, 0.03},
    {"capella", "Capella", 5.2782, 45.9979, 0.08},
    {"rigel", "Rigel", 5.2423, -8.2016, 0.13},
    {"procyon", "Procyon", 7.6550, 5.2250, 0.34},
    {"betelgeuse", "Betelgeuse", 5.9195, 7.4071, 0.42},
}};

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
        if (
            !isSunOrMoonType(body.type)
            && body.type != CelestialBodyType::Planet
            && body.type != CelestialBodyType::DeepSkyObject
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

    if (starCount == 0U) {
        for (const ReferenceLineStar& star : kReferenceLineStars) {
            if (catalog_identity::containsBodyId(result.bodies, star.id)) {
                continue;
            }

            CelestialBody body;
            body.id = star.id;
            body.displayName = star.displayName;
            body.type = CelestialBodyType::Star;
            body.visualMagnitude = star.visualMagnitude;
            body.fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = star.rightAscensionHours,
                .declinationDeg = star.declinationDeg
            };

            result.sourceKinds.push_back(CatalogCompositionSource::BuiltInEphemeris);
            result.bodies.push_back(std::move(body));
        }
    }

    return result;
}

}  // namespace skygate::ephemeris
