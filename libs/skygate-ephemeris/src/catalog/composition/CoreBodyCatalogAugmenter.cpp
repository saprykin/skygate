#include "CoreBodyCatalogAugmenter.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentity.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

struct BundledBrightStar final {
    std::string_view id;
    std::string_view displayName;
    double rightAscensionHours;
    double declinationDeg;
    double visualMagnitude;
};

constexpr std::array<BundledBrightStar, 8> kBundledBrightStars{{
    {"sirius", "Sirius", 6.7525, -16.7161, -1.46},
    {"canopus", "Canopus", 6.3992, -52.6957, -0.74},
    {"arcturus", "Arcturus", 14.2610, 19.1825, -0.05},
    {"vega", "Vega", 18.6156, 38.7837, 0.03},
    {"capella", "Capella", 5.2782, 45.9979, 0.08},
    {"rigel", "Rigel", 5.2423, -8.2016, 0.13},
    {"procyon", "Procyon", 7.6550, 5.2250, 0.34},
    {"betelgeuse", "Betelgeuse", 5.9195, 7.4071, 0.42},
}};

bool isSunOrMoonType(const BaseCelestialBody::Kind type)
{
    return type == BaseCelestialBody::Kind::Sun || type == BaseCelestialBody::Kind::Moon;
}

CatalogCompositionSource sourceKindForBody(const BaseCelestialBody& body)
{
    return CatalogIdentity::isAnalyticSolarSystemBody(body) ? CatalogCompositionSource::BuiltInEphemeris
                                                            : CatalogCompositionSource::Primary;
}

OwnGalaxyCelestialBody toOwnGalaxyBody(const BaseCelestialBody& body)
{
    OwnGalaxyCelestialBody ownGalaxyBody;
    ownGalaxyBody.id = body.id;
    ownGalaxyBody.displayName = body.displayName;
    ownGalaxyBody.kind = body.kind;
    ownGalaxyBody.visualMagnitude = body.visualMagnitude;
    ownGalaxyBody.fixedEquatorial = body.fixedEquatorialValue();
    ownGalaxyBody.starAstrometry = body.starAstrometryValue();
    return ownGalaxyBody;
}

bool containsBodyId(const std::span<const OwnGalaxyCelestialBody> bodies, const std::string_view id)
{
    return std::any_of(bodies.begin(), bodies.end(), [id](const OwnGalaxyCelestialBody& body) {
        return StringUtilities::equalsIgnoreAsciiCase(body.id, id);
    });
}

}  // namespace

CatalogAugmentationResult CoreBodyCatalogAugmenter::augment(const std::span<const BaseCelestialBody* const> bodies)
{
    CatalogAugmentationResult result;
    result.bodies.reserve(bodies.size());
    result.sourceKinds.reserve(result.bodies.size());
    for (const BaseCelestialBody* body : bodies) {
        if (body == nullptr || body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }
        result.bodies.push_back(toOwnGalaxyBody(*body));
        result.sourceKinds.push_back(sourceKindForBody(*body));
    }

    const std::size_t starCount = static_cast<std::size_t>(
        std::count_if(result.bodies.begin(), result.bodies.end(), [](const OwnGalaxyCelestialBody& body) {
            return body.kind == BaseCelestialBody::Kind::Star;
        })
    );

    std::unique_ptr<IStarCatalog> bundledCatalog = CatalogFactory::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return result;
    }

    for (const BaseCelestialBody* body : bundledCatalog->bodies()) {
        if (body == nullptr) {
            continue;
        }

        if (!isSunOrMoonType(body->kind) && body->kind != BaseCelestialBody::Kind::Planet
            && body->kind != BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }

        if (body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }

        if (containsBodyId(result.bodies, body->id)) {
            continue;
        }

        result.sourceKinds.push_back(sourceKindForBody(*body));
        result.bodies.push_back(toOwnGalaxyBody(*body));
    }

    if (starCount == 0U) {
        for (const BundledBrightStar& star : kBundledBrightStars) {
            if (containsBodyId(result.bodies, star.id)) {
                continue;
            }

            OwnGalaxyCelestialBody body;
            body.id = star.id;
            body.displayName = star.displayName;
            body.kind = BaseCelestialBody::Kind::Star;
            body.visualMagnitude = star.visualMagnitude;
            body.fixedEquatorial = core::EquatorialCoordinate{
                .rightAscensionHours = star.rightAscensionHours, .declinationDeg = star.declinationDeg
            };

            result.sourceKinds.push_back(CatalogCompositionSource::BuiltInEphemeris);
            result.bodies.push_back(std::move(body));
        }
    }

    return result;
}

}  // namespace skygate::ephemeris
