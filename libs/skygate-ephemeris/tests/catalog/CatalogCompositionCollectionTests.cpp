#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogCompositionSourceEntry;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

OwnGalaxyCelestialBody makeStar(
    std::string id,
    std::string displayName = {},
    std::vector<CatalogIdentifier> identifiers = {},
    std::optional<double> rightAscensionHours = 1.0,
    std::optional<double> declinationDeg = 2.0
)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::Star;
    body.identity.externalIdentifiers = std::move(identifiers);
    if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = *rightAscensionHours, .declinationDeg = *declinationDeg
        };
    }
    return body;
}

DistantCelestialBody makeDeepSkyObject(
    std::string id,
    std::string displayName = {},
    std::vector<std::string> aliases = {},
    std::vector<CatalogIdentifier> identifiers = {},
    std::optional<double> rightAscensionHours = 1.0,
    std::optional<double> declinationDeg = 2.0,
    std::optional<double> majorAxisArcmin = std::nullopt
)
{
    DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.identity.externalIdentifiers = std::move(identifiers);
    body.identity.aliases = aliases;
    if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = *rightAscensionHours, .declinationDeg = *declinationDeg
        };
    }
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = std::move(aliases),
        .majorAxisArcmin = majorAxisArcmin,
    };
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog>
createCatalog(std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies)
{
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyBodies.size() + distantBodies.size());
    for (std::size_t index = 0; index < ownGalaxyBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = index,
            }
        );
    }
    for (std::size_t index = 0; index < distantBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant,
                .bodyIndex = index,
            }
        );
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
}

const BaseCelestialBody* findBodyById(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    const auto it = std::find_if(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    });
    return it == bodies.end() ? nullptr : *it;
}

std::size_t countBodiesById(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    }));
}

std::size_t
countBodiesOfKind(const std::span<const BaseCelestialBody* const> bodies, const BaseCelestialBody::Kind kind)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [kind](const BaseCelestialBody* body) {
        return body != nullptr && body->kind == kind;
    }));
}

bool hasIdentifier(const BaseCelestialBody& body, const std::string_view namespaceName, const std::string_view value)
{
    return std::any_of(
        body.identity.externalIdentifiers.begin(),
        body.identity.externalIdentifiers.end(),
        [&](const CatalogIdentifier& identifier) {
            return identifier.namespaceName == namespaceName && identifier.value == value;
        }
    );
}

bool hasAlias(const std::vector<std::string>& aliases, const std::string_view alias)
{
    return std::find(aliases.begin(), aliases.end(), alias) != aliases.end();
}

std::optional<std::string> sourceIdFor(
    const CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view id
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            if (index >= result.sourceIds.size()) {
                return std::nullopt;
            }
            return result.sourceIds[index];
        }
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> contributorsFor(
    const CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view id
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            if (index >= result.contributorSourceIds.size()) {
                return std::nullopt;
            }
            return result.contributorSourceIds[index];
        }
    }
    return std::nullopt;
}

}  // namespace

class CatalogCompositionCollectionTests final : public QObject {
    Q_OBJECT

private slots:
    void composesTwoStarAndTwoDsoSourcesWithOverlapsAndMixedSource();
    void disabledSourceContributesNothing();
    void disappearingContributorRestoresEarlierWinner();
    void reorderingSourcesChangesWinners();
    void preservesStableSourceOrdering();
    void reportsDeterministicCounts();
    void reportsSourceRowAndPerKindCounts();
    void bundledAugmentationCarriesOwnProvenance();
    void deepSkyFallbackFillsGapsWithoutReplacingConfiguredValues();
    void deepSkyFallbackNeverOverridesReplacementOrder();
    void doesNotDuplicatePrimarySolarSystemBodies();
    void bundledBrightStarsOnlyAddedWhenNoStarsPresent();
    void usesCurrentConstellationCountWhenLarger();
    void ignoresNonDeepSkyRowsFromDeepSkySource();
    void reorderedBridgeSourcesKeepSingleWinnerAndCompleteContributors();
    void repeatedImportsKeepDeterministicWinnerAndContributors();
    void reimportedDescriptorReplacesAbsorbedRecord();
};

void CatalogCompositionCollectionTests::composesTwoStarAndTwoDsoSourcesWithOverlapsAndMixedSource()
{
    auto starA = createCatalog(
        {
            makeStar("a_hip_1", "Alpha A", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0),
            makeStar("a_hip_2", "Beta A", {CatalogIdentifier::make("hip", "2")}, 3.0, 4.0),
        },
        {}
    );
    auto starB = createCatalog(
        {
            makeStar("b_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, 5.0, 6.0),
            makeStar("b_hip_3", "Gamma B", {CatalogIdentifier::make("hip", "3")}, 7.0, 8.0),
        },
        {}
    );
    auto dsoA = createCatalog(
        {},
        {
            makeDeepSkyObject("a_ngc_224", "M31 A", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 178.0),
            makeDeepSkyObject("a_ngc_598", "M33 A", {"M33"}, {CatalogIdentifier::make("ngc", "598")}),
        }
    );
    auto dsoB = createCatalog(
        {},
        {
            makeDeepSkyObject(
                "b_ngc_224",
                "OpenNGC M31",
                {"M 31", "NGC 224"},
                {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("messier", "31")},
                std::nullopt,
                std::nullopt,
                std::nullopt
            ),
            makeDeepSkyObject("b_ngc_9999", "Edge case", {"NGC 9999"}, {CatalogIdentifier::make("ngc", "9999")}),
        }
    );
    auto mixed = createCatalog(
        {makeStar("mixed_hip_4", "Mixed star", {CatalogIdentifier::make("hip", "4")}, 9.0, 10.0)},
        {makeDeepSkyObject("mixed_ngc_1111", "Mixed DSO", {"NGC 1111"}, {CatalogIdentifier::make("ngc", "1111")})}
    );
    QVERIFY(starA != nullptr);
    QVERIFY(starB != nullptr);
    QVERIFY(dsoA != nullptr);
    QVERIFY(dsoB != nullptr);
    QVERIFY(mixed != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "star-a", .enabled = true, .catalog = starA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "star-b", .enabled = true, .catalog = starB.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "dso-a", .enabled = true, .catalog = dsoA.get(), .policy = CatalogCompositionPolicy::DeepSkyOnly},
        {.sourceId = "dso-b", .enabled = true, .catalog = dsoB.get(), .policy = CatalogCompositionPolicy::DeepSkyOnly},
        {.sourceId = "mixed", .enabled = true, .catalog = mixed.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.sourceIds.size(), bodies.size());

    // Star overlap: the later source wins and absorbs the earlier metadata.
    QCOMPARE(countBodiesById(bodies, "a_hip_1"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "b_hip_1"), std::size_t{1});
    const BaseCelestialBody* mergedStar = findBodyById(bodies, "b_hip_1");
    QVERIFY(mergedStar != nullptr);
    QCOMPARE(QString::fromStdString(mergedStar->displayName), QStringLiteral("Alpha A"));
    QVERIFY(mergedStar->fixedEquatorialValue().has_value());
    QCOMPARE(mergedStar->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(mergedStar->fixedEquatorialValue()->declinationDeg, 6.0);
    QCOMPARE(*sourceIdFor(result, bodies, "b_hip_1"), std::string("star-b"));
    const auto starContributors = contributorsFor(result, bodies, "b_hip_1");
    QVERIFY(starContributors.has_value());
    QCOMPARE(*starContributors, (std::vector<std::string>{"star-b", "star-a"}));

    // DSO overlap: the later source wins, identifiers/aliases/coordinates merge.
    QCOMPARE(countBodiesById(bodies, "a_ngc_224"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "b_ngc_224"), std::size_t{1});
    const BaseCelestialBody* mergedDso = findBodyById(bodies, "b_ngc_224");
    QVERIFY(mergedDso != nullptr);
    QCOMPARE(QString::fromStdString(mergedDso->displayName), QStringLiteral("OpenNGC M31"));
    QVERIFY(hasIdentifier(*mergedDso, "ngc", "224"));
    QVERIFY(hasIdentifier(*mergedDso, "messier", "031"));
    QVERIFY(hasAlias(mergedDso->identity.aliases, "M31"));
    QVERIFY(hasAlias(mergedDso->identity.aliases, "M 31"));
    QVERIFY(hasAlias(mergedDso->identity.aliases, "NGC 224"));
    QVERIFY(mergedDso->fixedEquatorialValue().has_value());
    const auto* dsoInfo = mergedDso->deepSkyObjectInfo();
    QVERIFY(dsoInfo != nullptr);
    QVERIFY(dsoInfo->majorAxisArcmin.has_value());
    QCOMPARE(*dsoInfo->majorAxisArcmin, 178.0);
    QCOMPARE(*sourceIdFor(result, bodies, "b_ngc_224"), std::string("dso-b"));
    const auto dsoContributors = contributorsFor(result, bodies, "b_ngc_224");
    QVERIFY(dsoContributors.has_value());
    QCOMPARE(*dsoContributors, (std::vector<std::string>{"dso-b", "dso-a"}));

    // Non-overlapping and mixed-source bodies survive with their provenance.
    QCOMPARE(countBodiesById(bodies, "a_hip_2"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "b_hip_3"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "mixed_hip_4"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_ngc_598"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "b_ngc_9999"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "mixed_ngc_1111"), std::size_t{1});
    QCOMPARE(*sourceIdFor(result, bodies, "mixed_hip_4"), std::string("mixed"));
    QCOMPARE(*sourceIdFor(result, bodies, "mixed_ngc_1111"), std::string("mixed"));

    QCOMPARE(countBodiesOfKind(bodies, BaseCelestialBody::Kind::Star), std::size_t{4});
    QCOMPARE(countBodiesOfKind(bodies, BaseCelestialBody::Kind::DeepSkyObject), std::size_t{4});
}

void CatalogCompositionCollectionTests::disabledSourceContributesNothing()
{
    auto primary = createCatalog({makeStar("hip_1", "Primary", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    auto supplement = createCatalog(
        {makeStar("supplement_hip_2", "Supplement", {CatalogIdentifier::make("hip", "2")}, 3.0, 4.0)}, {}
    );
    QVERIFY(primary != nullptr);
    QVERIFY(supplement != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = primary.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "supplement",
         .enabled = false,
         .catalog = supplement.get(),
         .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(countBodiesById(bodies, "hip_1"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "supplement_hip_2"), std::size_t{0});
}

void CatalogCompositionCollectionTests::disappearingContributorRestoresEarlierWinner()
{
    auto primary =
        createCatalog({makeStar("primary_hip_1", "Primary", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    auto overlay =
        createCatalog({makeStar("overlay_hip_1", "Overlay", {CatalogIdentifier::make("hip", "1")}, 5.0, 6.0)}, {});
    QVERIFY(primary != nullptr);
    QVERIFY(overlay != nullptr);

    const auto compose = [&](const bool overlayEnabled) {
        CatalogCompositionRequest request;
        request.sources = {
            {.sourceId = "primary",
             .enabled = true,
             .catalog = primary.get(),
             .policy = CatalogCompositionPolicy::Merge},
            {.sourceId = "overlay",
             .enabled = overlayEnabled,
             .catalog = overlay.get(),
             .policy = CatalogCompositionPolicy::Merge},
        };
        return skygate::ephemeris::CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult withOverlay = compose(true);
    QVERIFY(withOverlay.isSuccess());
    {
        const std::span<const BaseCelestialBody* const> bodies = withOverlay.catalog->bodies();
        QCOMPARE(countBodiesById(bodies, "overlay_hip_1"), std::size_t{1});
        QCOMPARE(countBodiesById(bodies, "primary_hip_1"), std::size_t{0});
        const BaseCelestialBody* winner = findBodyById(bodies, "overlay_hip_1");
        QVERIFY(winner != nullptr);
        QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 5.0);
    }

    // Disabling the overlay restores the primary contributor's object.
    const CatalogCompositionResult withoutOverlay = compose(false);
    QVERIFY(withoutOverlay.isSuccess());
    {
        const std::span<const BaseCelestialBody* const> bodies = withoutOverlay.catalog->bodies();
        QCOMPARE(countBodiesById(bodies, "overlay_hip_1"), std::size_t{0});
        QCOMPARE(countBodiesById(bodies, "primary_hip_1"), std::size_t{1});
        const BaseCelestialBody* winner = findBodyById(bodies, "primary_hip_1");
        QVERIFY(winner != nullptr);
        QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
        QCOMPARE(*sourceIdFor(withoutOverlay, bodies, "primary_hip_1"), std::string("primary"));
    }
}

void CatalogCompositionCollectionTests::reorderingSourcesChangesWinners()
{
    auto first = createCatalog({makeStar("first_hip_1", "First", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    auto second =
        createCatalog({makeStar("second_hip_1", "Second", {CatalogIdentifier::make("hip", "1")}, 5.0, 6.0)}, {});
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);

    const auto compose = [&](const bool secondFirst) {
        CatalogCompositionRequest request;
        if (secondFirst) {
            request.sources = {
                {.sourceId = "second",
                 .enabled = true,
                 .catalog = second.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "first",
                 .enabled = true,
                 .catalog = first.get(),
                 .policy = CatalogCompositionPolicy::Merge},
            };
        } else {
            request.sources = {
                {.sourceId = "first",
                 .enabled = true,
                 .catalog = first.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "second",
                 .enabled = true,
                 .catalog = second.get(),
                 .policy = CatalogCompositionPolicy::Merge},
            };
        }
        return skygate::ephemeris::CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult secondLast = compose(false);
    QVERIFY(secondLast.isSuccess());
    QCOMPARE(countBodiesById(secondLast.catalog->bodies(), "second_hip_1"), std::size_t{1});
    QCOMPARE(countBodiesById(secondLast.catalog->bodies(), "first_hip_1"), std::size_t{0});

    const CatalogCompositionResult firstLast = compose(true);
    QVERIFY(firstLast.isSuccess());
    QCOMPARE(countBodiesById(firstLast.catalog->bodies(), "first_hip_1"), std::size_t{1});
    QCOMPARE(countBodiesById(firstLast.catalog->bodies(), "second_hip_1"), std::size_t{0});
}

void CatalogCompositionCollectionTests::preservesStableSourceOrdering()
{
    auto sourceA = createCatalog({makeStar("a_star_1", {}, {}, 1.0, 2.0), makeStar("a_star_2", {}, {}, 3.0, 4.0)}, {});
    auto sourceB = createCatalog({makeStar("b_star_3", {}, {}, 5.0, 6.0)}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    std::vector<std::string> orderedIds;
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    orderedIds.reserve(bodies.size());
    for (const BaseCelestialBody* body : bodies) {
        if (body != nullptr) {
            orderedIds.push_back(body->id);
        }
    }
    const std::vector<std::string> expected = {"a_star_1", "a_star_2", "b_star_3"};
    QCOMPARE(orderedIds, expected);
}

void CatalogCompositionCollectionTests::reportsDeterministicCounts()
{
    auto stars = createCatalog({makeStar("star_1", {}, {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    auto deepSky = createCatalog(
        {},
        {
            makeDeepSkyObject("dso_1", {}, {"NGC 1"}, {CatalogIdentifier::make("ngc", "1")}),
            makeDeepSkyObject("dso_2", {}, {"NGC 2"}, {CatalogIdentifier::make("ngc", "2")}),
        }
    );
    QVERIFY(stars != nullptr);
    QVERIFY(deepSky != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "stars", .enabled = true, .catalog = stars.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "deep-sky",
         .enabled = true,
         .catalog = deepSky.get(),
         .policy = CatalogCompositionPolicy::DeepSkyOnly},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{3});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(result.foundDeepSkyObjectCount, std::size_t{2});

    // A source-reported pre-merge count is preserved over the computed figure.
    request.knownDeepSkyObjectCount = 41U;
    const CatalogCompositionResult withKnownCount = skygate::ephemeris::CatalogComposer::composeCollection(request);
    QVERIFY(withKnownCount.isSuccess());
    QCOMPARE(withKnownCount.foundDeepSkyObjectCount, std::size_t{41});
}

void CatalogCompositionCollectionTests::reportsSourceRowAndPerKindCounts()
{
    auto mixed = createCatalog(
        {
            makeStar("mixed_hip_1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0),
            makeStar("mixed_hip_2", "Beta", {CatalogIdentifier::make("hip", "2")}, 3.0, 4.0),
        },
        {
            makeDeepSkyObject("mixed_ngc_224", "M31", {"M31"}, {CatalogIdentifier::make("ngc", "224")}),
        }
    );
    auto deepSky = createCatalog(
        {},
        {
            makeDeepSkyObject("dso_ngc_598", "M33", {"M33"}, {CatalogIdentifier::make("ngc", "598")}),
        }
    );
    QVERIFY(mixed != nullptr);
    QVERIFY(deepSky != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "mixed", .enabled = true, .catalog = mixed.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "deep-sky",
         .enabled = true,
         .catalog = deepSky.get(),
         .policy = CatalogCompositionPolicy::DeepSkyOnly},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{4});
    QCOMPARE(result.starCount, std::size_t{2});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(result.planetCount, std::size_t{0});
    QCOMPARE(result.moonCount, std::size_t{0});
    QCOMPARE(result.sunCount, std::size_t{0});

    QCOMPARE(result.sourceOrder, (std::vector<std::string>{"mixed", "deep-sky"}));
    QCOMPARE(result.sourceRowCounts, (std::vector<std::size_t>{3U, 1U}));
}

void CatalogCompositionCollectionTests::bundledAugmentationCarriesOwnProvenance()
{
    OwnGalaxyCelestialBody constellation;
    constellation.id = "constellation_orion";
    constellation.displayName = "Orion";
    constellation.kind = BaseCelestialBody::Kind::Constellation;
    auto primary = createCatalog({std::move(constellation)}, {});
    QVERIFY(primary != nullptr);

    auto bundledCore = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(bundledCore != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = primary.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "bundled-core",
         .enabled = true,
         .catalog = bundledCore.get(),
         .policy = CatalogCompositionPolicy::AugmentCore},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();

    const BaseCelestialBody* sirius = findBodyById(bodies, "sirius");
    QVERIFY(sirius != nullptr);
    QCOMPARE(*sourceIdFor(result, bodies, "sirius"), std::string("bundled-core"));

    const BaseCelestialBody* orion = findBodyById(bodies, "constellation_orion");
    QVERIFY(orion != nullptr);
    QCOMPARE(*sourceIdFor(result, bodies, "constellation_orion"), std::string("primary"));
}

void CatalogCompositionCollectionTests::deepSkyFallbackFillsGapsWithoutReplacingConfiguredValues()
{
    DistantCelestialBody configuredM31 =
        makeDeepSkyObject("messier_031", "Configured M31", {"M31"}, {CatalogIdentifier::make("messier", "31")});
    configuredM31.visualMagnitude = 0.0;
    auto configured = createCatalog(
        {makeStar("configured_hip_1", "Configured star", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)},
        {configuredM31}
    );
    QVERIFY(configured != nullptr);

    DistantCelestialBody fallbackM31 =
        makeDeepSkyObject("messier_031", "Bundled M31", {"M31"}, {CatalogIdentifier::make("messier", "31")});
    fallbackM31.visualMagnitude = 3.44;
    DistantCelestialBody fallbackOnly =
        makeDeepSkyObject("ngc_9999", "Fallback only", {"NGC 9999"}, {CatalogIdentifier::make("ngc", "9999")});
    fallbackOnly.visualMagnitude = 9.0;
    auto fallback = createCatalog({}, {fallbackM31, fallbackOnly});
    QVERIFY(fallback != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "configured",
         .enabled = true,
         .catalog = configured.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "bundled-deep-sky",
         .enabled = true,
         .catalog = fallback.get(),
         .policy = CatalogCompositionPolicy::DeepSkyFallback},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);
    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();

    // The fallback fills gaps only: the configured M31 keeps its magnitude and
    // its provenance.
    const BaseCelestialBody* m31 = findBodyById(bodies, "messier_031");
    QVERIFY(m31 != nullptr);
    QCOMPARE(m31->visualMagnitude, 0.0);
    QVERIFY(sourceIdFor(result, bodies, "messier_031").has_value());
    QCOMPARE(*sourceIdFor(result, bodies, "messier_031"), std::string("configured"));
    QVERIFY(contributorsFor(result, bodies, "messier_031").has_value());
    QCOMPARE(*contributorsFor(result, bodies, "messier_031"), (std::vector<std::string>{"configured"}));

    // An identity no configured source supplies comes from the fallback and
    // carries the fallback's own source id.
    QVERIFY(findBodyById(bodies, "ngc_9999") != nullptr);
    QVERIFY(sourceIdFor(result, bodies, "ngc_9999").has_value());
    QCOMPARE(*sourceIdFor(result, bodies, "ngc_9999"), std::string("bundled-deep-sky"));
    QVERIFY(contributorsFor(result, bodies, "ngc_9999").has_value());
    QCOMPARE(*contributorsFor(result, bodies, "ngc_9999"), (std::vector<std::string>{"bundled-deep-sky"}));
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    // The composer derives the pre-merge deep-sky count from the participating
    // deep-sky sources, including a DeepSkyFallback source.
    QCOMPARE(result.foundDeepSkyObjectCount, std::size_t{2});
}

void CatalogCompositionCollectionTests::deepSkyFallbackNeverOverridesReplacementOrder()
{
    DistantCelestialBody configuredM31 =
        makeDeepSkyObject("messier_031", "Configured M31", {"M31"}, {CatalogIdentifier::make("messier", "31")});
    configuredM31.visualMagnitude = 0.0;
    auto configured = createCatalog({}, {configuredM31});
    QVERIFY(configured != nullptr);

    DistantCelestialBody fallbackM31 =
        makeDeepSkyObject("messier_031", "Bundled M31", {"M31"}, {CatalogIdentifier::make("messier", "31")});
    fallbackM31.visualMagnitude = 3.44;
    auto fallback = createCatalog({}, {fallbackM31});
    QVERIFY(fallback != nullptr);

    DistantCelestialBody replacementM31 =
        makeDeepSkyObject("messier_031", "Replacement M31", {"M31"}, {CatalogIdentifier::make("messier", "31")});
    replacementM31.visualMagnitude = 5.0;
    auto replacement = createCatalog({}, {replacementM31});
    QVERIFY(replacement != nullptr);

    const CatalogCompositionSourceEntry configuredEntry{
        .sourceId = "configured",
        .enabled = true,
        .catalog = configured.get(),
        .policy = CatalogCompositionPolicy::Merge
    };
    const CatalogCompositionSourceEntry fallbackEntry{
        .sourceId = "bundled-deep-sky",
        .enabled = true,
        .catalog = fallback.get(),
        .policy = CatalogCompositionPolicy::DeepSkyFallback
    };
    const CatalogCompositionSourceEntry replacementEntry{
        .sourceId = "replacement",
        .enabled = true,
        .catalog = replacement.get(),
        .policy = CatalogCompositionPolicy::DeepSkyOnly
    };

    const auto composeWithSources = [](std::vector<CatalogCompositionSourceEntry> sources) {
        CatalogCompositionRequest request;
        request.sources = std::move(sources);
        return skygate::ephemeris::CatalogComposer::composeCollection(request);
    };

    // The fallback is not the last position in the request, and the later
    // explicit replacement source still wins over it and over the configured
    // source.
    const CatalogCompositionResult fallbackBefore =
        composeWithSources({configuredEntry, fallbackEntry, replacementEntry});
    QVERIFY(fallbackBefore.isSuccess());
    const BaseCelestialBody* beforeM31 = findBodyById(fallbackBefore.catalog->bodies(), "messier_031");
    QVERIFY(beforeM31 != nullptr);
    QCOMPARE(beforeM31->visualMagnitude, 5.0);
    QVERIFY(sourceIdFor(fallbackBefore, fallbackBefore.catalog->bodies(), "messier_031").has_value());
    QCOMPARE(*sourceIdFor(fallbackBefore, fallbackBefore.catalog->bodies(), "messier_031"), std::string("replacement"));

    // Placing the fallback after the replacement source does not change the
    // winner either: gap-fill never replaces an existing survivor.
    const CatalogCompositionResult fallbackAfter =
        composeWithSources({configuredEntry, replacementEntry, fallbackEntry});
    QVERIFY(fallbackAfter.isSuccess());
    const BaseCelestialBody* afterM31 = findBodyById(fallbackAfter.catalog->bodies(), "messier_031");
    QVERIFY(afterM31 != nullptr);
    QCOMPARE(afterM31->visualMagnitude, 5.0);
    QVERIFY(sourceIdFor(fallbackAfter, fallbackAfter.catalog->bodies(), "messier_031").has_value());
    QCOMPARE(*sourceIdFor(fallbackAfter, fallbackAfter.catalog->bodies(), "messier_031"), std::string("replacement"));
}

void CatalogCompositionCollectionTests::doesNotDuplicatePrimarySolarSystemBodies()
{
    OwnGalaxyCelestialBody sun;
    sun.id = "sun";
    sun.kind = BaseCelestialBody::Kind::Sun;
    OwnGalaxyCelestialBody moon;
    moon.id = "moon";
    moon.kind = BaseCelestialBody::Kind::Moon;
    OwnGalaxyCelestialBody mars;
    mars.id = "mars";
    mars.kind = BaseCelestialBody::Kind::Planet;

    auto primary = createCatalog({std::move(sun), std::move(moon), std::move(mars)}, {});
    QVERIFY(primary != nullptr);
    auto bundledCore = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(bundledCore != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = primary.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "bundled-core",
         .enabled = true,
         .catalog = bundledCore.get(),
         .policy = CatalogCompositionPolicy::AugmentCore},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(countBodiesById(bodies, "sun"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "moon"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "mars"), std::size_t{1});
}

void CatalogCompositionCollectionTests::bundledBrightStarsOnlyAddedWhenNoStarsPresent()
{
    OwnGalaxyCelestialBody constellation;
    constellation.id = "constellation_orion";
    constellation.kind = BaseCelestialBody::Kind::Constellation;

    auto noStars = createCatalog({std::move(constellation)}, {});
    QVERIFY(noStars != nullptr);
    auto withStars = createCatalog({makeStar("hip_1", "HIP 1", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    QVERIFY(withStars != nullptr);
    auto bundledCore = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(bundledCore != nullptr);

    const auto compose = [&](const skygate::ephemeris::IStarCatalog* source) {
        CatalogCompositionRequest request;
        request.sources = {
            {.sourceId = "primary", .enabled = true, .catalog = source, .policy = CatalogCompositionPolicy::Merge},
            {.sourceId = "bundled-core",
             .enabled = true,
             .catalog = bundledCore.get(),
             .policy = CatalogCompositionPolicy::AugmentCore},
        };
        return skygate::ephemeris::CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult withoutStars = compose(noStars.get());
    QVERIFY(withoutStars.isSuccess());
    QVERIFY(findBodyById(withoutStars.catalog->bodies(), "sirius") != nullptr);

    const CatalogCompositionResult withStarResult = compose(withStars.get());
    QVERIFY(withStarResult.isSuccess());
    QVERIFY(findBodyById(withStarResult.catalog->bodies(), "sirius") == nullptr);
}

void CatalogCompositionCollectionTests::usesCurrentConstellationCountWhenLarger()
{
    auto primary = createCatalog({makeStar("hip_1", "HIP 1", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    QVERIFY(primary != nullptr);

    CatalogCompositionRequest request;
    request.currentConstellationCount = 12U;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = primary.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.constellationCount, std::size_t{12});
}

void CatalogCompositionCollectionTests::ignoresNonDeepSkyRowsFromDeepSkySource()
{
    auto primary = createCatalog({makeStar("hip_1", "HIP 1", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    QVERIFY(primary != nullptr);
    auto deepSky = createCatalog(
        {makeStar("hip_bad", "Not Deep Sky", {CatalogIdentifier::make("hip", "2")}, 1.0, 2.0)},
        {makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"}, {CatalogIdentifier::make("ngc", "1")})}
    );
    QVERIFY(deepSky != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = primary.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "deep-sky",
         .enabled = true,
         .catalog = deepSky.get(),
         .policy = CatalogCompositionPolicy::DeepSkyOnly},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QVERIFY(findBodyById(bodies, "hip_bad") == nullptr);
    QVERIFY(findBodyById(bodies, "ngc_1") != nullptr);
}

void CatalogCompositionCollectionTests::reorderedBridgeSourcesKeepSingleWinnerAndCompleteContributors()
{
    // Source A knows HIP 1, source C knows HD 2, and source B carries both
    // identifiers, so B is the bridge no matter where it appears in the
    // collection. Every order must produce one survivor whose contributor list
    // covers all three sources; which source wins follows the documented
    // precedence: because every source matches the established object, the
    // last source of the order supplies the survivor.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    const auto sourceB = createCatalog(
        {makeStar(
            "b_bridge", "Bridge", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceC =
        createCatalog({makeStar("c_hd2", "Gamma", {CatalogIdentifier::make("hd", "2")}, 1.0, 2.0)}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    struct ExpectedOutcome final {
        std::vector<std::size_t> order;
        std::string winnerId;
        std::vector<std::string> contributors;
    };

    const std::vector<std::string> sourceIds = {"source-a", "source-b", "source-c"};
    const std::vector<std::string> rowIds = {"a_hip1", "b_bridge", "c_hd2"};
    const std::vector<const skygate::ephemeris::IStarCatalog*> catalogs = {sourceA.get(), sourceB.get(), sourceC.get()};
    const std::vector<ExpectedOutcome> outcomes = {
        {{0U, 1U, 2U}, "c_hd2", {"source-c", "source-b", "source-a"}},
        {{0U, 2U, 1U}, "b_bridge", {"source-b", "source-a", "source-c"}},
        {{1U, 0U, 2U}, "c_hd2", {"source-c", "source-a", "source-b"}},
        {{1U, 2U, 0U}, "a_hip1", {"source-a", "source-c", "source-b"}},
        {{2U, 0U, 1U}, "b_bridge", {"source-b", "source-c", "source-a"}},
        {{2U, 1U, 0U}, "a_hip1", {"source-a", "source-b", "source-c"}},
    };

    for (const ExpectedOutcome& outcome : outcomes) {
        CatalogCompositionRequest request;
        request.sources.reserve(outcome.order.size());
        for (const std::size_t catalogIndex : outcome.order) {
            request.sources.push_back(
                CatalogCompositionSourceEntry{
                    .sourceId = sourceIds[catalogIndex],
                    .enabled = true,
                    .catalog = catalogs[catalogIndex],
                    .policy = CatalogCompositionPolicy::Merge,
                }
            );
        }

        const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);
        QVERIFY(result.isSuccess());
        QCOMPARE(result.bodyCount, std::size_t{1});
        QCOMPARE(result.starCount, std::size_t{1});

        const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
        const BaseCelestialBody* winner = findBodyById(bodies, outcome.winnerId);
        QVERIFY2(winner != nullptr, qPrintable(QString::fromStdString(outcome.winnerId)));
        QVERIFY(hasIdentifier(*winner, "hip", "1"));
        QVERIFY(hasIdentifier(*winner, "hd", "2"));
        QCOMPARE(*sourceIdFor(result, bodies, outcome.winnerId), outcome.contributors.front());
        QCOMPARE(*contributorsFor(result, bodies, outcome.winnerId), outcome.contributors);

        for (const std::string& rowId : rowIds) {
            if (rowId != outcome.winnerId) {
                QCOMPARE(countBodiesById(bodies, rowId), std::size_t{0});
            }
        }
    }
}

void CatalogCompositionCollectionTests::repeatedImportsKeepDeterministicWinnerAndContributors()
{
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    const auto reloadedA =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    const auto sourceB = createCatalog(
        {makeStar(
            "b_bridge", "Bridge", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceC =
        createCatalog({makeStar("c_hd2", "Gamma", {CatalogIdentifier::make("hd", "2")}, 1.0, 2.0)}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(reloadedA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const auto compose = [&](const skygate::ephemeris::IStarCatalog* sourceACatalog) {
        CatalogCompositionRequest request;
        request.sources = {
            {.sourceId = "source-a",
             .enabled = true,
             .catalog = sourceACatalog,
             .policy = CatalogCompositionPolicy::Merge},
            {.sourceId = "source-b",
             .enabled = true,
             .catalog = sourceB.get(),
             .policy = CatalogCompositionPolicy::Merge},
            {.sourceId = "source-c",
             .enabled = true,
             .catalog = sourceC.get(),
             .policy = CatalogCompositionPolicy::Merge},
        };
        return skygate::ephemeris::CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult first = compose(sourceA.get());
    const CatalogCompositionResult repeated = compose(sourceA.get());
    const CatalogCompositionResult afterReload = compose(reloadedA.get());

    for (const CatalogCompositionResult* result : {&first, &repeated, &afterReload}) {
        QVERIFY(result->isSuccess());
        QCOMPARE(result->bodyCount, std::size_t{1});
        QCOMPARE(result->starCount, std::size_t{1});

        const std::span<const BaseCelestialBody* const> bodies = result->catalog->bodies();
        const BaseCelestialBody* winner = findBodyById(bodies, "c_hd2");
        QVERIFY(winner != nullptr);
        QVERIFY(hasIdentifier(*winner, "hip", "1"));
        QVERIFY(hasIdentifier(*winner, "hd", "2"));
        QCOMPARE(*sourceIdFor(*result, bodies, "c_hd2"), std::string("source-c"));
        QCOMPARE(
            *contributorsFor(*result, bodies, "c_hd2"), (std::vector<std::string>{"source-c", "source-b", "source-a"})
        );
    }
}

void CatalogCompositionCollectionTests::reimportedDescriptorReplacesAbsorbedRecord()
{
    // Two instances of one descriptor describe the same record key. The later
    // instance replaces the absorbed record instead of adding a survivor, and
    // its own source instance stays the winner.
    const auto firstImport =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    const auto secondImport =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, 1.0, 2.0)}, {});
    QVERIFY(firstImport != nullptr);
    QVERIFY(secondImport != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a",
         .enabled = true,
         .catalog = firstImport.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-a-reload",
         .enabled = true,
         .catalog = secondImport.get(),
         .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = skygate::ephemeris::CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_hip1"), std::size_t{1});
    QCOMPARE(*sourceIdFor(result, bodies, "a_hip1"), std::string("source-a-reload"));
    QCOMPARE(*contributorsFor(result, bodies, "a_hip1"), (std::vector<std::string>{"source-a-reload", "source-a"}));
}

QTEST_APPLESS_MAIN(CatalogCompositionCollectionTests)

#include "CatalogCompositionCollectionTests.moc"
