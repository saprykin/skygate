#include "CoreBodyCatalogAugmenter.hpp"

#include "CelestialBodyCatalog.hpp"
#include "catalog/CatalogCompositionMerger.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/InMemoryStarCatalog.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

}  // namespace

CatalogAugmentationResult CoreBodyCatalogAugmenter::augment(const std::span<const BaseCelestialBody* const> bodies)
{
    // Express the legacy single-slot augmentation as an ordered source
    // collection and delegate to the authoritative collection merge. The
    // primary slot contributes every body kind; the bundled catalog is the
    // explicit AugmentCore source that gap-fills non-deep-sky bodies and
    // enables the bright-star fallback.
    CelestialBodyCatalog primaryCatalog(bodies);
    InMemoryStarCatalog primarySource(std::move(primaryCatalog));

    CatalogCompositionRequest request;
    request.sources.reserve(2U);
    request.sources.push_back(
        CatalogCompositionSourceEntry{
            .sourceId = std::string(CatalogCompositionMerger::sourceKindId(CatalogCompositionSource::Primary)),
            .enabled = true,
            .catalog = &primarySource,
            .policy = CatalogCompositionPolicy::Merge,
        }
    );

    std::unique_ptr<IStarCatalog> bundledCatalog = CatalogFactory::createBundledStarCatalog();
    if (bundledCatalog != nullptr) {
        request.sources.push_back(
            CatalogCompositionSourceEntry{
                .sourceId =
                    std::string(CatalogCompositionMerger::sourceKindId(CatalogCompositionSource::BuiltInEphemeris)),
                .enabled = true,
                .catalog = bundledCatalog.get(),
                .policy = CatalogCompositionPolicy::AugmentCore,
            }
        );
    }

    CatalogCompositionMergeResult merged = CatalogCompositionMerger::mergeCollection(request);

    CatalogAugmentationResult result;
    result.sourceKinds.reserve(merged.ownGalaxyBodies.size());
    for (std::size_t position = 0; position < merged.orderedBodyIndexes.size(); ++position) {
        const CelestialBodyCatalog::OrderEntry& orderEntry = merged.orderedBodyIndexes[position];
        if (orderEntry.domain != CelestialBodyCatalog::BodyDomain::OwnGalaxy) {
            continue;
        }

        const OwnGalaxyCelestialBody& body = merged.ownGalaxyBodies[orderEntry.bodyIndex];
        if (CatalogIdentity::isAnalyticSolarSystemBody(body)) {
            result.sourceKinds.push_back(CatalogCompositionSource::BuiltInEphemeris);
            continue;
        }
        result.sourceKinds.push_back(CatalogCompositionMerger::sourceKindFromId(merged.sourceIds[position]));
    }
    result.bodies = std::move(merged.ownGalaxyBodies);
    return result;
}

std::vector<OwnGalaxyCelestialBody> CoreBodyCatalogAugmenter::bundledBrightStars()
{
    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.reserve(kBundledBrightStars.size());
    for (const BundledBrightStar& star : kBundledBrightStars) {
        OwnGalaxyCelestialBody body;
        body.id = star.id;
        body.displayName = star.displayName;
        body.kind = BaseCelestialBody::Kind::Star;
        body.visualMagnitude = star.visualMagnitude;
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = star.rightAscensionHours, .declinationDeg = star.declinationDeg
        };
        bodies.push_back(std::move(body));
    }
    return bodies;
}

}  // namespace skygate::ephemeris
