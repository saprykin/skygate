#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "catalog/IStarCatalog.hpp"
#include "time/EphemerisDateRange.hpp"

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

using skygate::core::AstronomicalEpoch;
using skygate::core::EquatorialCoordinate;
using skygate::core::TimeScale;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogComposer;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::EphemerisDateRange;
using skygate::ephemeris::IStarCatalog;
using skygate::ephemeris::OwnGalaxyCelestialBody;

OwnGalaxyCelestialBody makeStar(
    std::string id,
    std::string displayName = {},
    std::vector<CatalogIdentifier> identifiers = {},
    std::vector<std::string> aliases = {},
    std::optional<double> rightAscensionHours = 1.0,
    std::optional<double> declinationDeg = 2.0
)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::Star;
    body.identity.externalIdentifiers = std::move(identifiers);
    body.identity.aliases = std::move(aliases);
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

CatalogCompositionResult composePrimary(const skygate::ephemeris::IStarCatalog& source)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = &source, .policy = CatalogCompositionPolicy::Merge},
    };
    return CatalogComposer::composeCollection(request);
}

CatalogCompositionResult composePrimaryWithDeepSky(
    const skygate::ephemeris::IStarCatalog& source, const skygate::ephemeris::IStarCatalog& deepSky
)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = &source, .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "deep-sky", .enabled = true, .catalog = &deepSky, .policy = CatalogCompositionPolicy::DeepSkyOnly},
    };
    return CatalogComposer::composeCollection(request);
}

// Composes one replacing source with one gap-fill source of the given policy.
// The gap-fill source is configured after the replacing source, so it never
// outranks the survivor it is supposed to fill gaps around.
CatalogCompositionResult composeWithGapFill(
    const skygate::ephemeris::IStarCatalog& source,
    const skygate::ephemeris::IStarCatalog& gapFill,
    const CatalogCompositionPolicy gapFillPolicy
)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = &source, .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "gap-fill", .enabled = true, .catalog = &gapFill, .policy = gapFillPolicy},
    };
    return CatalogComposer::composeCollection(request);
}

// Composes the two sources in collection order; the second source owns the
// higher merge precedence.
CatalogCompositionResult
composeInOrder(const skygate::ephemeris::IStarCatalog& first, const skygate::ephemeris::IStarCatalog& second)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "first", .enabled = true, .catalog = &first, .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "second", .enabled = true, .catalog = &second, .policy = CatalogCompositionPolicy::Merge},
    };
    return CatalogComposer::composeCollection(request);
}

// Composes every catalog in collection order as an enabled Merge source with a
// position-derived source identity, so multi-stage replacement and bridge
// chains stay readable.
CatalogCompositionResult composeAll(const std::vector<const IStarCatalog*>& catalogs)
{
    CatalogCompositionRequest request;
    request.sources.reserve(catalogs.size());
    for (std::size_t index = 0; index < catalogs.size(); ++index) {
        request.sources.push_back(
            {.sourceId = "source-" + std::to_string(index),
             .enabled = true,
             .catalog = catalogs[index],
             .policy = CatalogCompositionPolicy::Merge}
        );
    }
    return CatalogComposer::composeCollection(request);
}

[[nodiscard]] AstronomicalEpoch j2000Epoch()
{
    return {
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.0,
        .timeScale = TimeScale::Tt,
    };
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

bool hasRetainedCanonicalId(const BaseCelestialBody& body, const std::string_view retainedId)
{
    return std::find(body.identity.retainedCanonicalIds.begin(), body.identity.retainedCanonicalIds.end(), retainedId)
           != body.identity.retainedCanonicalIds.end();
}

// Sorted identifier keys, so two survivors can be compared independently of the
// order in which the composition accumulated them.
std::vector<std::string> sortedIdentifierKeys(const BaseCelestialBody& body)
{
    std::vector<std::string> keys;
    keys.reserve(body.identity.externalIdentifiers.size());
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        keys.push_back(identifier.key());
    }
    std::ranges::sort(keys);
    return keys;
}

std::vector<std::string> sortedRetainedCanonicalIds(const BaseCelestialBody& body)
{
    std::vector<std::string> retainedIds = body.identity.retainedCanonicalIds;
    std::ranges::sort(retainedIds);
    return retainedIds;
}

}  // namespace

class CatalogIdentityMergeTests final : public QObject {
    Q_OBJECT

private slots:
    void deduplicatesDuplicateHipStars();
    void deduplicatesDuplicateOpenNgcRows();
    void keepsSameNameUnrelatedObjectsDistinct();
    void keepsAmbiguousAliasMatchesDistinct();
    void mergesAuthoritativeMatchesWithDeepSkyPrecedence();
    void resolvesIdentifierChainsWithinSource();
    void keepsIncompatibleKindsDistinct();
    void keepsWinnerCoordinatesOverConflictingAstrometry();
    void rejectsLosingAstrometryThatContradictsTheWinningFixedPosition();
    void keepsOneCoherentCoordinateModelWhenConflictingSourcesAreReordered();
    void adoptsCompatibleLosingAstrometryForAFixedPositionWinner();
    void fillsMissingAstrometryFieldsAcrossCompatibleEpochs();
    void fillsMissingAstrometryFieldsFromAnUndeclaredEpoch();
    void rejectsLosingAstrometryThatDisagreesAfterEpochConversion();
    void rejectsLosingFixedCoordinatesThatContradictWinningAstrometry();
    void fillsMissingMetadataWithoutDiscardingWinnerValues();
    void preservesStableOrdering();
    void mergesLargeFixtureWithoutAllPairsScan();
    void keepsSharedCommonNameDesignationsDistinct();
    void keepsFallbackDesignationsDistinctWhenOnlyTheCommonNameMatches();
    void matchingFallbackRowPreservesConfiguredWinnerValues();
    void keepsFallbackIncompatibleKindDistinct();
    void augmentCoreKeepsBundledBodyDistinctFromConfiguredDeepSkyObject();
    void keepsAmbiguousFallbackAliasMatchDistinct();
    void matchesCaseAndPaddingDesignationVariants();
    void keepsSuffixDesignationsDistinct();
    void mergesExplicitCrossIdentifications();
    void primaryDesignationSurvivesBinaryRoundTrip();
    void bridgesIdentifiersAcquiredEarlierInTheSameSourcePass();
    void bridgesAmbiguousAuthoritativeIdentifiersIntoSingleSurvivor();
    void keepsIncompatibleKindsDistinctWhileBridgingSameKindSurvivors();
    void bridgesDeepSkyObjectsWithMetadataUnion();
    void bridgeInheritsMissingMetadataFromTheLatestContributor();
    void bridgeWinnerKeepsItsOwnMetadata();
    void bridgeInheritsOptionalStarMetadataFromTheLatestContributor();
    void reorderingBridgeSourcesChangesInheritedMetadata();
    void bridgeAbsorptionUsesSourcePrecedenceAfterReplacements();
    void multiStageBridgesRetainEveryContributorOnce();
    void bridgePrefersTheNameOfItsActualSupplyingSource();
    void bridgePrefersTheDeepSkyAxisOfItsActualSupplyingSource();
    void explicitReplacementValueOutranksAnInheritedValue();
    void deepSkyValuesResolveIndependentlyBySupplyingSource();
    void absentAndZeroDeepSkyValuesKeepTheirMeaning();
    void inheritedAstrometryValuesKeepTheirSupplyingSource();
    void inheritedFixedCoordinatesFollowTheirSupplyingSource();
    void reorderedReplacementChainChangesTheInheritedName();
    void retainsReplacedCanonicalIdentityAcrossLaterSources();
    void retainsBridgedCanonicalIdentitiesAcrossLaterSources();
    void retainedCanonicalIdentitySurvivesBinaryRoundTripAndRecomposition();
    void combinedChainKeepsIdentityMetadataAndProvenanceAcrossSnapshotRecomposition();
};

void CatalogIdentityMergeTests::deduplicatesDuplicateHipStars()
{
    const auto parsed = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "hip,ra,dec,mag\n"
        "123,1.0,2.0,3.0\n"
        "123,2.0,3.0,4.0\n"
    );
    QVERIFY(parsed.isSuccess());
    QVERIFY(parsed.catalog != nullptr);

    const auto result = composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(countBodiesById(result.catalog->bodies(), "hip_123"), std::size_t{1});
}

void CatalogIdentityMergeTests::deduplicatesDuplicateOpenNgcRows()
{
    const auto primary = createCatalog({makeStar("hip_1")}, {});
    const auto deepSky = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;M;NGC;IC;Identifiers;Common names\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky.isSuccess());
    QVERIFY(deepSky.catalog != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "messier_031"), std::size_t{1});
}

void CatalogIdentityMergeTests::keepsSameNameUnrelatedObjectsDistinct()
{
    const auto primary = createCatalog({makeStar("hip_1")}, {});
    const auto deepSky = createCatalog(
        {},
        {
            makeDeepSkyObject("open_ngc_first", "Andromeda", {}, {}),
            makeDeepSkyObject("open_ngc_second", "Andromeda", {}, {}),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "open_ngc_first"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "open_ngc_second"), std::size_t{1});
}

void CatalogIdentityMergeTests::keepsAmbiguousAliasMatchesDistinct()
{
    const auto primary = createCatalog({makeStar("hip_1")}, {});
    const auto deepSky = createCatalog(
        {},
        {
            makeDeepSkyObject("ngc_224", "Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}),
            makeDeepSkyObject("ngc_598", "Triangulum", {"M31"}, {CatalogIdentifier::make("ngc", "598")}),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_224"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_598"), std::size_t{1});
}

void CatalogIdentityMergeTests::mergesAuthoritativeMatchesWithDeepSkyPrecedence()
{
    const auto primary = createCatalog(
        {},
        {makeDeepSkyObject(
            "ngc_224", "Primary Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 3.4
        )}
    );
    const auto deepSky = createCatalog(
        {},
        {makeDeepSkyObject(
            "open_ngc_m31",
            "OpenNGC Andromeda",
            {"M 31", "NGC 224"},
            {CatalogIdentifier::make("messier", "31"), CatalogIdentifier::make("ngc", "224")},
            1.0,
            2.0,
            3.4
        )}
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(merged != nullptr);
    QCOMPARE(QString::fromStdString(merged->displayName), QStringLiteral("OpenNGC Andromeda"));
    QVERIFY(hasIdentifier(*merged, "messier", "031"));
    QVERIFY(hasIdentifier(*merged, "ngc", "224"));
    QVERIFY(hasAlias(merged->identity.aliases, "M 31"));
    QVERIFY(hasAlias(merged->identity.aliases, "NGC 224"));
    QVERIFY(hasAlias(merged->identity.aliases, "M31"));
}

void CatalogIdentityMergeTests::resolvesIdentifierChainsWithinSource()
{
    const auto source = createCatalog(
        {
            makeStar("cat_a_1", {}, {CatalogIdentifier::make("hip", "1")}),
            makeStar("cat_a_2", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "5")}),
            makeStar("cat_a_3", {}, {CatalogIdentifier::make("hyg", "5")}),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const auto result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(countBodiesById(result.catalog->bodies(), "cat_a_1"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "cat_a_2"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "cat_a_3"), std::size_t{0});

    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "cat_a_1");
    QVERIFY(merged != nullptr);
    QVERIFY(hasIdentifier(*merged, "hip", "1"));
    QVERIFY(hasIdentifier(*merged, "hyg", "5"));
}

void CatalogIdentityMergeTests::keepsIncompatibleKindsDistinct()
{
    const auto source = createCatalog(
        {makeStar("hip_123", {}, {CatalogIdentifier::make("hip", "123")})},
        {makeDeepSkyObject("ngc_123", {}, {}, {CatalogIdentifier::make("hip", "123")})}
    );
    QVERIFY(source != nullptr);

    const auto result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(countBodiesById(result.catalog->bodies(), "hip_123"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_123"), std::size_t{1});

    const BaseCelestialBody* star = findBodyById(result.catalog->bodies(), "hip_123");
    const BaseCelestialBody* deepSkyObject = findBodyById(result.catalog->bodies(), "ngc_123");
    QVERIFY(star != nullptr);
    QVERIFY(deepSkyObject != nullptr);
    QCOMPARE(star->kind, BaseCelestialBody::Kind::Star);
    QCOMPARE(deepSkyObject->kind, BaseCelestialBody::Kind::DeepSkyObject);
}

void CatalogIdentityMergeTests::keepsWinnerCoordinatesOverConflictingAstrometry()
{
    const auto source = createCatalog(
        {
            makeStar("hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0),
            makeStar("hip_1_dup", {}, {CatalogIdentifier::make("hip", "1")}, {}, 5.0, 6.0),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const auto result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(countBodiesById(result.catalog->bodies(), "hip_1"), std::size_t{1});

    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "hip_1");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->fixedEquatorialValue().has_value());
    QCOMPARE(merged->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(merged->fixedEquatorialValue()->declinationDeg, 2.0);
}

void CatalogIdentityMergeTests::rejectsLosingAstrometryThatContradictsTheWinningFixedPosition()
{
    // The earlier source places the shared HIP 1 object at RA 1 with
    // astrometry at the same default reference epoch. The later source fixes
    // the object at RA 10 and carries no astrometry. The later source wins the
    // position, so its fixed coordinates survive and the unrelated astrometry
    // is rejected instead of being copied beside them.
    OwnGalaxyCelestialBody earlier = makeStar("star_low", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody later = makeStar("star_high", {}, {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0);

    const auto earlierSource = createCatalog({earlier}, {});
    const auto laterSource = createCatalog({later}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_high over conflicting fixed coordinates from star_low."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_high and rejected the incompatible astrometry of "
        "star_low."
    );

    const CatalogCompositionResult result = composeInOrder(*earlierSource, *laterSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "star_high"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "star_low"), std::size_t{0});

    const BaseCelestialBody* winner = findBodyById(result.catalog->bodies(), "star_high");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 20.0);
    QVERIFY(!winner->starAstrometryValue().has_value());
}

void CatalogIdentityMergeTests::keepsOneCoherentCoordinateModelWhenConflictingSourcesAreReordered()
{
    // The same two records in the opposite collection order: the later source
    // now owns the coherent fixed plus astrometry model, so it wins whole and
    // only the losing fixed coordinates are rejected. Reordering sources never
    // publishes a survivor whose fixed position and reference position
    // disagree.
    OwnGalaxyCelestialBody astrometric = makeStar("star_low", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    astrometric.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *astrometric.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody fixedOnly = makeStar("star_high", {}, {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0);

    const auto fixedSource = createCatalog({fixedOnly}, {});
    const auto astrometricSource = createCatalog({astrometric}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_low over conflicting fixed coordinates from star_high."
    );

    const CatalogCompositionResult result = composeInOrder(*fixedSource, *astrometricSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* winner = findBodyById(result.catalog->bodies(), "star_low");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(winner->starAstrometryValue().has_value());
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(*winner->starAstrometryValue()->properMotionRightAscensionMasPerYear, 125.0);
}

void CatalogIdentityMergeTests::adoptsCompatibleLosingAstrometryForAFixedPositionWinner()
{
    // The winner's fixed position and the losing reference position agree, so
    // the losing astrometry enriches the survivor instead of being dropped.
    OwnGalaxyCelestialBody donor = makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "2")}, {}, 5.0, -10.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *donor.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
        .radialVelocityKmPerSecond = -5.5,
        .validityRange = EphemerisDateRange{
            .id = "donor-validity",
            .displayName = "Donor validity",
            .start = j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody fixedOnly =
        makeStar("star_fixed", {}, {CatalogIdentifier::make("hip", "2")}, {}, 5.0, -10.0);

    const auto donorSource = createCatalog({donor}, {});
    const auto fixedSource = createCatalog({fixedOnly}, {});

    const CatalogCompositionResult result = composeInOrder(*donorSource, *fixedSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* winner = findBodyById(result.catalog->bodies(), "star_fixed");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, -10.0);

    QVERIFY(winner->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *winner->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, -10.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 125.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -55.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 7.5);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, -5.5);
    QVERIFY(astrometry.validityRange.has_value());
    QCOMPARE(QString::fromStdString(astrometry.validityRange->id), QStringLiteral("donor-validity"));
}

void CatalogIdentityMergeTests::fillsMissingAstrometryFieldsAcrossCompatibleEpochs()
{
    // The donor reference position lies 50 years of proper motion away from the
    // winner reference position, expressed in the catalog's mas/year units.
    // The losing proper motion reconciles the epoch difference, so the losing
    // optional fields are adopted while the winner's reference position, epoch,
    // and existing proper motion stay authoritative. A rate-unit error (for
    // example reading mas/year as arcseconds per year) would leave a residual
    // many degrees wide and reject this compatible model.
    constexpr double kEpochGapYears = 50.0;
    constexpr double kProperMotionRaMasPerYear = 1'000.0;
    constexpr double kProperMotionDecMasPerYear = -500.0;
    const double donorRaOffsetHours = kProperMotionRaMasPerYear * kEpochGapYears / 3'600'000.0 / 15.0;
    const double donorDecOffsetDeg = kProperMotionDecMasPerYear * kEpochGapYears / 3'600'000.0;

    OwnGalaxyCelestialBody donor = makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            EquatorialCoordinate{.rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = donorDecOffsetDeg},
        .referenceEpoch =
            AstronomicalEpoch{
                .julianDatePart1 = j2000Epoch().julianDatePart1,
                .julianDatePart2 = kEpochGapYears * 365.25,
                .timeScale = TimeScale::Tt,
            },
        .properMotionRightAscensionMasPerYear = kProperMotionRaMasPerYear,
        .properMotionDeclinationMasPerYear = kProperMotionDecMasPerYear,
        .stellarParallaxMas = 5.0,
        .radialVelocityKmPerSecond = 10.0,
        .validityRange = EphemerisDateRange{
            .id = "donor-validity",
            .displayName = "Donor validity",
            .start = j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody winner = makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = kProperMotionRaMasPerYear,
    };

    const auto donorSource = createCatalog({donor}, {});
    const auto winnerSource = createCatalog({winner}, {});

    const CatalogCompositionResult result = composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "star_winner");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *merged->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 0.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, kProperMotionRaMasPerYear);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, kProperMotionDecMasPerYear);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 5.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, 10.0);
    QVERIFY(astrometry.validityRange.has_value());
    QCOMPARE(QString::fromStdString(astrometry.validityRange->id), QStringLiteral("donor-validity"));
}

void CatalogIdentityMergeTests::fillsMissingAstrometryFieldsFromAnUndeclaredEpoch()
{
    // A reference position that declares no epoch claims the same epoch as the
    // winner's declared reference epoch, so no epoch conversion applies and
    // agreeing positions still enrich the winner.
    OwnGalaxyCelestialBody donor = makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "5")}, {}, 2.0, 3.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *donor.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 500.0,
        .stellarParallaxMas = 6.0,
        .radialVelocityKmPerSecond = -3.0,
    };
    OwnGalaxyCelestialBody winner = makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "5")}, {}, 2.0, 3.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 250.0,
    };

    const auto donorSource = createCatalog({donor}, {});
    const auto winnerSource = createCatalog({winner}, {});

    const CatalogCompositionResult result = composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "star_winner");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *merged->starAstrometryValue();
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 250.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 6.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, -3.0);
}

void CatalogIdentityMergeTests::rejectsLosingAstrometryThatDisagreesAfterEpochConversion()
{
    // Same epoch gap and reference offset as the compatible case, but the
    // losing proper motion is twice the rate the offset implies. The epoch
    // conversion leaves a 50 arcsecond residual, so the models describe
    // different directions and the losing astrometry does not enrich the
    // winner.
    constexpr double kEpochGapYears = 50.0;
    constexpr double kImpliedProperMotionRaMasPerYear = 1'000.0;
    const double donorRaOffsetHours = kImpliedProperMotionRaMasPerYear * kEpochGapYears / 3'600'000.0 / 15.0;

    OwnGalaxyCelestialBody donor = makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            EquatorialCoordinate{.rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = 0.0},
        .referenceEpoch =
            AstronomicalEpoch{
                .julianDatePart1 = j2000Epoch().julianDatePart1,
                .julianDatePart2 = kEpochGapYears * 365.25,
                .timeScale = TimeScale::Tt,
            },
        .properMotionRightAscensionMasPerYear = 2.0 * kImpliedProperMotionRaMasPerYear,
        .properMotionDeclinationMasPerYear = -500.0,
        .stellarParallaxMas = 5.0,
        .radialVelocityKmPerSecond = 10.0,
        .validityRange = EphemerisDateRange{
            .id = "donor-validity",
            .displayName = "Donor validity",
            .start = j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody winner = makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = kImpliedProperMotionRaMasPerYear,
    };

    const auto donorSource = createCatalog({donor}, {});
    const auto winnerSource = createCatalog({winner}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of star_winner and rejected the incompatible astrometry of "
        "star_donor."
    );

    const CatalogCompositionResult result = composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "star_winner");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *merged->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, kImpliedProperMotionRaMasPerYear);
    QVERIFY(!astrometry.properMotionDeclinationMasPerYear.has_value());
    QVERIFY(!astrometry.stellarParallaxMas.has_value());
    QVERIFY(!astrometry.radialVelocityKmPerSecond.has_value());
    QVERIFY(!astrometry.validityRange.has_value());
}

void CatalogIdentityMergeTests::rejectsLosingFixedCoordinatesThatContradictWinningAstrometry()
{
    // The winner (later source) carries astrometry at RA 1 and no fixed
    // position. The losing record's fixed position at RA 10 is unrelated, so it
    // is rejected instead of contradicting the winning reference coordinates.
    OwnGalaxyCelestialBody fixedOnly =
        makeStar("star_fixed", {}, {CatalogIdentifier::make("hip", "4")}, {}, 10.0, 20.0);
    OwnGalaxyCelestialBody astrometric = makeStar("star_astrometric", {}, {CatalogIdentifier::make("hip", "4")});
    astrometric.fixedEquatorial.reset();
    astrometric.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
    };

    const auto fixedSource = createCatalog({fixedOnly}, {});
    const auto astrometricSource = createCatalog({astrometric}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of star_astrometric and rejected the incompatible fixed "
        "coordinates of star_fixed."
    );

    const CatalogCompositionResult result = composeInOrder(*fixedSource, *astrometricSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* winner = findBodyById(result.catalog->bodies(), "star_astrometric");
    QVERIFY(winner != nullptr);
    QVERIFY(!winner->fixedEquatorialValue().has_value());
    QVERIFY(winner->starAstrometryValue().has_value());
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
}

void CatalogIdentityMergeTests::fillsMissingMetadataWithoutDiscardingWinnerValues()
{
    const auto primary = createCatalog(
        {},
        {makeDeepSkyObject(
            "ngc_224", "Primary Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 10.0
        )}
    );
    const auto deepSky = createCatalog(
        {},
        {makeDeepSkyObject(
            "open_ngc_m31",
            "OpenNGC Andromeda",
            {"M 31"},
            {CatalogIdentifier::make("messier", "31"), CatalogIdentifier::make("ngc", "224")},
            std::nullopt,
            std::nullopt,
            std::nullopt
        )}
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(merged != nullptr);
    QCOMPARE(QString::fromStdString(merged->displayName), QStringLiteral("OpenNGC Andromeda"));
    QVERIFY(merged->fixedEquatorialValue().has_value());
    QCOMPARE(merged->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(merged->fixedEquatorialValue()->declinationDeg, 2.0);

    const auto* deepSkyInfo = merged->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 10.0);
    QVERIFY(hasAlias(merged->identity.aliases, "M31"));
    QVERIFY(hasAlias(merged->identity.aliases, "M 31"));
}

void CatalogIdentityMergeTests::preservesStableOrdering()
{
    const auto primary = createCatalog(
        {
            makeStar("star_a"),
            makeStar("star_b"),
        },
        {makeDeepSkyObject("dso_primary", "Primary DSO", {"M31"}, {CatalogIdentifier::make("messier", "31")})}
    );
    const auto deepSky = createCatalog(
        {},
        {
            makeDeepSkyObject("dso_new", "New DSO", {}, {CatalogIdentifier::make("ngc", "9999")}),
            makeDeepSkyObject("dso_replacement", "Replacement", {"M 31"}, {CatalogIdentifier::make("messier", "31")}),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());

    std::vector<std::string> orderedIds;
    orderedIds.reserve(result.catalog->bodies().size());
    for (const BaseCelestialBody* body : result.catalog->bodies()) {
        if (body != nullptr) {
            orderedIds.push_back(body->id);
        }
    }

    const std::vector<std::string> expectedPrefix = {"star_a", "star_b"};
    QVERIFY(orderedIds.size() >= expectedPrefix.size());
    for (std::size_t index = 0; index < expectedPrefix.size(); ++index) {
        QCOMPARE(QString::fromStdString(orderedIds[index]), QString::fromStdString(expectedPrefix[index]));
    }

    const auto dsoNewPosition = std::find(orderedIds.begin(), orderedIds.end(), "dso_new");
    const auto dsoReplacementPosition = std::find(orderedIds.begin(), orderedIds.end(), "dso_replacement");
    QVERIFY(dsoNewPosition != orderedIds.end());
    QVERIFY(dsoReplacementPosition != orderedIds.end());
    QVERIFY(dsoNewPosition < dsoReplacementPosition);
    QVERIFY(std::find(orderedIds.begin(), orderedIds.end(), "dso_primary") == orderedIds.end());
}

void CatalogIdentityMergeTests::mergesLargeFixtureWithoutAllPairsScan()
{
    constexpr std::size_t kFixtureSize = 6000;

    std::vector<DistantCelestialBody> primaryDso;
    std::vector<DistantCelestialBody> deepSkyDso;
    primaryDso.reserve(kFixtureSize);
    deepSkyDso.reserve(kFixtureSize);

    for (std::size_t index = 0; index < kFixtureSize; ++index) {
        const std::string ngc = std::to_string(100000 + index);
        primaryDso.push_back(makeDeepSkyObject(
            "primary_" + ngc, "Primary " + ngc, {"NGC " + ngc}, {CatalogIdentifier::make("ngc", ngc)}, 1.0, 2.0
        ));
        deepSkyDso.push_back(makeDeepSkyObject(
            "secondary_" + ngc,
            "Secondary " + ngc,
            {"NGC " + ngc},
            {CatalogIdentifier::make("ngc", ngc), CatalogIdentifier::make("ic", ngc)},
            1.0,
            2.0
        ));
    }

    const auto primary = createCatalog({}, std::move(primaryDso));
    const auto deepSky = createCatalog({}, std::move(deepSkyDso));
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, kFixtureSize);

    const BaseCelestialBody* first = findBodyById(result.catalog->bodies(), "secondary_100000");
    QVERIFY(first != nullptr);
    QVERIFY(hasIdentifier(*first, "ngc", "100000"));
    QVERIFY(hasIdentifier(*first, "ic", "100000"));

    const BaseCelestialBody* last = findBodyById(result.catalog->bodies(), "secondary_105999");
    QVERIFY(last != nullptr);
    QVERIFY(hasIdentifier(*last, "ngc", "105999"));
    QVERIFY(hasIdentifier(*last, "ic", "105999"));

    QCOMPARE(countBodiesById(result.catalog->bodies(), "primary_100000"), std::size_t{0});
}

void CatalogIdentityMergeTests::keepsSharedCommonNameDesignationsDistinct()
{
    const auto parsed = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0001;G;01:00:00;+02:00:00;Shared region\n"
        "NGC0002;G;10:00:00;+20:00:00;Shared region\n"
    );
    QVERIFY(parsed.isSuccess());
    QVERIFY(parsed.catalog != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_2 distinct from ngc_1 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    const auto result = composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_1"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_2"), std::size_t{1});

    const BaseCelestialBody* first = findBodyById(result.catalog->bodies(), "ngc_1");
    const BaseCelestialBody* second = findBodyById(result.catalog->bodies(), "ngc_2");
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(hasIdentifier(*first, "ngc", "1"));
    QVERIFY(hasIdentifier(*second, "ngc", "2"));
    QVERIFY(hasAlias(first->identity.aliases, "Shared region"));
    QVERIFY(hasAlias(second->identity.aliases, "Shared region"));
    QVERIFY(first->fixedEquatorialValue().has_value());
    QVERIFY(second->fixedEquatorialValue().has_value());
    QCOMPARE(first->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(second->fixedEquatorialValue()->rightAscensionHours, 10.0);
}

void CatalogIdentityMergeTests::keepsFallbackDesignationsDistinctWhenOnlyTheCommonNameMatches()
{
    // Both records carry a recognized NGC designation and share only the
    // descriptive common name, so the fallback row describes a different
    // object. A gap-fill policy must apply the same contradiction check as a
    // replacing source and append the fallback row instead of dropping it.
    const auto primary = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0001;G;01:00:00;+02:00:00;Shared region\n"
    );
    const auto fallback = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0002;G;10:00:00;+20:00:00;Shared region\n"
    );
    QVERIFY(primary.isSuccess());
    QVERIFY(fallback.isSuccess());
    QVERIFY(primary.catalog != nullptr);
    QVERIFY(fallback.catalog != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_2 distinct from ngc_1 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    const auto result =
        composeWithGapFill(*primary.catalog, *fallback.catalog, CatalogCompositionPolicy::DeepSkyFallback);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(countBodiesById(bodies, "ngc_1"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "ngc_2"), std::size_t{1});

    const BaseCelestialBody* configured = findBodyById(bodies, "ngc_1");
    const BaseCelestialBody* appended = findBodyById(bodies, "ngc_2");
    QVERIFY(configured != nullptr);
    QVERIFY(appended != nullptr);
    QVERIFY(hasIdentifier(*configured, "ngc", "1"));
    QVERIFY(hasIdentifier(*appended, "ngc", "2"));
    QVERIFY(configured->fixedEquatorialValue().has_value());
    QVERIFY(appended->fixedEquatorialValue().has_value());
    QCOMPARE(configured->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(appended->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.sourceIds.back(), std::string("gap-fill"));
}

void CatalogIdentityMergeTests::matchingFallbackRowPreservesConfiguredWinnerValues()
{
    // The fallback row carries the same authoritative designation, so the
    // shared identity decision proves both records describe one object.
    // Gap-fill preserves the configured survivor: the fallback's conflicting
    // values, alias, and provenance never replace or corrupt the winner.
    const auto primary = createCatalog(
        {},
        {makeDeepSkyObject(
            "ngc_224", "Configured Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 100.0
        )}
    );
    const auto fallback = createCatalog(
        {},
        {makeDeepSkyObject(
            "ngc_224",
            "Bundled Andromeda",
            {"M31", "Bundled alias"},
            {CatalogIdentifier::make("ngc", "224")},
            10.0,
            20.0,
            5.0
        )}
    );
    QVERIFY(primary != nullptr);
    QVERIFY(fallback != nullptr);

    const auto result = composeWithGapFill(*primary, *fallback, CatalogCompositionPolicy::DeepSkyFallback);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(bodies.size(), std::size_t{1});

    const BaseCelestialBody* winner = findBodyById(bodies, "ngc_224");
    QVERIFY(winner != nullptr);
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Configured Andromeda"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 2.0);
    const auto* info = winner->deepSkyObjectInfo();
    QVERIFY(info != nullptr);
    QVERIFY(info->majorAxisArcmin.has_value());
    QCOMPARE(*info->majorAxisArcmin, 100.0);
    QVERIFY(!hasAlias(winner->identity.aliases, "Bundled alias"));
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::keepsFallbackIncompatibleKindDistinct()
{
    // The fallback deep-sky row resolves through an authoritative identifier
    // shared with a configured star. Incompatible kinds stay distinct with
    // the normal diagnostic instead of silently suppressing the fallback row.
    const auto primary =
        createCatalog({makeStar("hip_123", "Configured star", {CatalogIdentifier::make("hip", "123")})}, {});
    const auto fallback = createCatalog(
        {}, {makeDeepSkyObject("fallback_shared_hip", "Fallback row", {}, {CatalogIdentifier::make("hip", "123")})}
    );
    QVERIFY(primary != nullptr);
    QVERIFY(fallback != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept fallback_shared_hip distinct from hip_123 because the shared identity is used by "
        "incompatible object kinds."
    );
    const auto result = composeWithGapFill(*primary, *fallback, CatalogCompositionPolicy::DeepSkyFallback);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "hip_123"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "fallback_shared_hip"), std::size_t{1});

    const BaseCelestialBody* star = findBodyById(bodies, "hip_123");
    const BaseCelestialBody* deepSky = findBodyById(bodies, "fallback_shared_hip");
    QVERIFY(star != nullptr);
    QVERIFY(deepSky != nullptr);
    QCOMPARE(star->kind, BaseCelestialBody::Kind::Star);
    QCOMPARE(deepSky->kind, BaseCelestialBody::Kind::DeepSkyObject);
}

void CatalogIdentityMergeTests::augmentCoreKeepsBundledBodyDistinctFromConfiguredDeepSkyObject()
{
    // A configured deep-sky record whose canonical id collides with a bundled
    // core body of another kind must not suppress the bundled augmentation.
    const auto primary = createCatalog({}, {makeDeepSkyObject("Mars", "Mars deep sky", {"Mars region"})});
    const auto bundledCore = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(primary != nullptr);
    QVERIFY(bundledCore != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept mars distinct from Mars because the shared identity is used by incompatible "
        "object kinds."
    );
    const auto result = composeWithGapFill(*primary, *bundledCore, CatalogCompositionPolicy::AugmentCore);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(countBodiesById(bodies, "Mars"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "mars"), std::size_t{1});

    const BaseCelestialBody* configured = findBodyById(bodies, "Mars");
    const BaseCelestialBody* bundled = findBodyById(bodies, "mars");
    QVERIFY(configured != nullptr);
    QVERIFY(bundled != nullptr);
    QCOMPARE(configured->kind, BaseCelestialBody::Kind::DeepSkyObject);
    QCOMPARE(bundled->kind, BaseCelestialBody::Kind::Planet);
}

void CatalogIdentityMergeTests::keepsAmbiguousFallbackAliasMatchDistinct()
{
    // The configured collection already has two different objects sharing one
    // weak alias, so a fallback row resolving only through that alias cannot
    // be proven equivalent to either. The row stays a distinct object with the
    // same diagnostic the normal merge path reports for an ambiguous match.
    const auto primary = createCatalog(
        {},
        {
            makeDeepSkyObject("ngc_224", "Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}),
            makeDeepSkyObject("ngc_598", "Triangulum", {"M31"}, {CatalogIdentifier::make("ngc", "598")}),
        }
    );
    const auto fallback = createCatalog({}, {makeDeepSkyObject("fallback_shared_alias", "Fallback row", {"M31"})});
    QVERIFY(primary != nullptr);
    QVERIFY(fallback != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_598 distinct from ngc_224 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept fallback_shared_alias distinct because its identity matched 2 different bodies."
    );
    const auto result = composeWithGapFill(*primary, *fallback, CatalogCompositionPolicy::DeepSkyFallback);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.deepSkyObjectCount, std::size_t{3});
    QCOMPARE(countBodiesById(bodies, "ngc_224"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "ngc_598"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "fallback_shared_alias"), std::size_t{1});
}

void CatalogIdentityMergeTests::matchesCaseAndPaddingDesignationVariants()
{
    const auto padded = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0001;G;01:00:00;+02:00:00;First region\n"
    );
    const auto unpadded = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "ngc 0001;G;01:00:00;+02:00:00;Second region\n"
    );
    QVERIFY(padded.isSuccess());
    QVERIFY(unpadded.isSuccess());

    const auto result = composePrimaryWithDeepSky(*padded.catalog, *unpadded.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "ngc_1");
    QVERIFY(merged != nullptr);
    QVERIFY(hasIdentifier(*merged, "ngc", "1"));
    QVERIFY(hasAlias(merged->identity.aliases, "First region"));
    QVERIFY(hasAlias(merged->identity.aliases, "Second region"));
}

void CatalogIdentityMergeTests::keepsSuffixDesignationsDistinct()
{
    const auto parsed = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0001;G;01:00:00;+02:00:00;Shared region\n"
        "NGC0001A;G;01:00:00;+02:00:00;Shared region\n"
    );
    QVERIFY(parsed.isSuccess());
    QVERIFY(parsed.catalog != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_1A distinct from ngc_1 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    const auto result = composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_1"), std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "ngc_1A"), std::size_t{1});

    const BaseCelestialBody* component = findBodyById(result.catalog->bodies(), "ngc_1A");
    QVERIFY(component != nullptr);
    QVERIFY(hasIdentifier(*component, "ngc", "1a"));
}

void CatalogIdentityMergeTests::mergesExplicitCrossIdentifications()
{
    const auto openNgc = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;M;NGC;IC;Identifiers;Common names\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
    );
    QVERIFY(openNgc.isSuccess());

    const auto referring = createCatalog(
        {},
        {makeDeepSkyObject(
            "external_ngc_224", "External NGC 224", {}, {CatalogIdentifier::make("ngc", "224")}, 0.7123, 41.269
        )}
    );
    QVERIFY(referring != nullptr);

    const auto result = composePrimaryWithDeepSky(*openNgc.catalog, *referring);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged = findBodyById(result.catalog->bodies(), "external_ngc_224");
    QVERIFY(merged != nullptr);
    QVERIFY(hasIdentifier(*merged, "ngc", "224"));
    QVERIFY(hasIdentifier(*merged, "messier", "031"));
    QVERIFY(hasAlias(merged->identity.aliases, "Andromeda Galaxy"));
}

void CatalogIdentityMergeTests::primaryDesignationSurvivesBinaryRoundTrip()
{
    const auto parsed = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;Common names\n"
        "NGC0001;G;01:00:00;+02:00:00;Shared region\n"
    );
    QVERIFY(parsed.isSuccess());
    QVERIFY(parsed.catalog != nullptr);

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(parsed.catalog->catalog());
    QVERIFY(!payload.isEmpty());

    const std::unique_ptr<skygate::ephemeris::IStarCatalog> restored =
        skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const BaseCelestialBody* restoredBody = findBodyById(restored->bodies(), "ngc_1");
    QVERIFY(restoredBody != nullptr);
    QVERIFY(hasIdentifier(*restoredBody, "ngc", "1"));

    const auto referring = createCatalog(
        {}, {makeDeepSkyObject("external_ngc_1", "External NGC 1", {}, {CatalogIdentifier::make("ngc", "1")})}
    );
    QVERIFY(referring != nullptr);

    const auto result = composePrimaryWithDeepSky(*restored, *referring);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "external_ngc_1"), std::size_t{1});
}

void CatalogIdentityMergeTests::bridgesIdentifiersAcquiredEarlierInTheSameSourcePass()
{
    // Source A establishes that HIP 1 and HD 2 are one object. Source B's
    // first record replaces A and acquires HD 2; its later record resolves
    // through that acquired identifier instead of appending a second survivor.
    const auto sourceA = createCatalog(
        {makeStar(
            "a_hip1_hd2",
            "Established",
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")},
            {},
            1.0,
            2.0
        )},
        {}
    );
    const auto sourceB = createCatalog(
        {
            makeStar("b_hip1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0),
            makeStar(
                "b_hd2",
                "Later record",
                {CatalogIdentifier::make("hd", "2"), CatalogIdentifier::make("hyg", "5")},
                {},
                1.0,
                2.0
            ),
        },
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_hip1_hd2"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "b_hd2"), std::size_t{0});

    const BaseCelestialBody* winner = findBodyById(bodies, "b_hip1");
    QVERIFY(winner != nullptr);
    QVERIFY(hasIdentifier(*winner, "hip", "1"));
    QVERIFY(hasIdentifier(*winner, "hd", "2"));
    // b_hd2 was merged into the survivor, so its own identifier survives too.
    QVERIFY(hasIdentifier(*winner, "hyg", "5"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Established"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-a"}));
}

void CatalogIdentityMergeTests::bridgesAmbiguousAuthoritativeIdentifiersIntoSingleSurvivor()
{
    // The bridging record carries both authoritative identifiers. It resolves
    // each of them to a different survivor of an earlier source, so it becomes
    // the single survivor of all three records instead of a third object.
    //
    // Contract correction: the bridge carries no display name of
    // its own, so its missing name comes from the highest-priority contributor,
    // and contributors are listed from the winner down to the lowest-priority
    // source. Source precedence is the configured collection position, not the
    // accumulator position of an absorbed survivor: source-c is configured
    // after source-a, so `Gamma` wins over `Alpha` and source-c precedes
    // source-a. The earlier version of this test asserted accumulator position
    // order, which is not source precedence.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceC =
        createCatalog({makeStar("c_hd2", "Gamma", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceB = createCatalog(
        {makeStar(
            "b_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceC != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_hip1"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "c_hd2"), std::size_t{0});

    const BaseCelestialBody* winner = findBodyById(bodies, "b_bridge");
    QVERIFY(winner != nullptr);
    QVERIFY(hasIdentifier(*winner, "hip", "1"));
    QVERIFY(hasIdentifier(*winner, "hd", "2"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Gamma"));
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-c", "source-a"}));
}

void CatalogIdentityMergeTests::keepsIncompatibleKindsDistinctWhileBridgingSameKindSurvivors()
{
    const auto stars =
        createCatalog({makeStar("star_a", {}, {CatalogIdentifier::make("hip", "123")}, {}, 1.0, 2.0)}, {});
    const auto conflictingDeepSky = createCatalog(
        {}, {makeDeepSkyObject("dso_hip_123", "Conflicting DSO", {}, {CatalogIdentifier::make("hip", "123")}, 1.0, 2.0)}
    );
    const auto starBridge =
        createCatalog({makeStar("star_bridge", {}, {CatalogIdentifier::make("hip", "123")}, {}, 1.0, 2.0)}, {});
    QVERIFY(stars != nullptr);
    QVERIFY(conflictingDeepSky != nullptr);
    QVERIFY(starBridge != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept dso_hip_123 distinct from star_a because the shared identity is used by "
        "incompatible object kinds."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept star_bridge distinct from dso_hip_123 because the shared identity is used by "
        "incompatible object kinds."
    );

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "stars", .enabled = true, .catalog = stars.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "conflicting-deep-sky",
         .enabled = true,
         .catalog = conflictingDeepSky.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "star-bridge",
         .enabled = true,
         .catalog = starBridge.get(),
         .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{2});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "star_a"), std::size_t{0});

    const BaseCelestialBody* bridgedStar = findBodyById(bodies, "star_bridge");
    QVERIFY(bridgedStar != nullptr);
    QVERIFY(hasIdentifier(*bridgedStar, "hip", "123"));
    const BaseCelestialBody* conflicting = findBodyById(bodies, "dso_hip_123");
    QVERIFY(conflicting != nullptr);
    QCOMPARE(conflicting->kind, BaseCelestialBody::Kind::DeepSkyObject);
    QVERIFY(hasIdentifier(*conflicting, "hip", "123"));
}

void CatalogIdentityMergeTests::bridgesDeepSkyObjectsWithMetadataUnion()
{
    // The bridge carries no display name and no major axis of its own, so the
    // missing deep-sky metadata comes from the highest-priority contributor.
    // Contract correction: source-c is configured after source-a,
    // so `C Triangulum` and its major axis win over `A Andromeda` and 178.0,
    // and source-c precedes source-a in the contributor list. Identifiers and
    // aliases still union from every contributor.
    const auto sourceA = createCatalog(
        {},
        {makeDeepSkyObject(
            "a_ngc_224", "A Andromeda", {"Andromeda Galaxy"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 178.0
        )}
    );
    const auto sourceC = createCatalog(
        {},
        {makeDeepSkyObject(
            "c_messier_31", "C Triangulum", {"Triangulum"}, {CatalogIdentifier::make("messier", "31")}, 1.0, 2.0, 60.0
        )}
    );
    const auto sourceB = createCatalog(
        {},
        {makeDeepSkyObject(
            "b_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("messier", "31")},
            std::nullopt,
            std::nullopt,
            std::nullopt
        )}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceC != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_ngc_224"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "c_messier_31"), std::size_t{0});

    const BaseCelestialBody* winner = findBodyById(bodies, "b_bridge");
    QVERIFY(winner != nullptr);
    QVERIFY(hasIdentifier(*winner, "ngc", "224"));
    QVERIFY(hasIdentifier(*winner, "messier", "031"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("C Triangulum"));
    QVERIFY(hasAlias(winner->identity.aliases, "Andromeda Galaxy"));
    QVERIFY(hasAlias(winner->identity.aliases, "Triangulum"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    const auto* deepSkyInfo = winner->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 60.0);
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-c", "source-a"}));
}

void CatalogIdentityMergeTests::bridgeInheritsMissingMetadataFromTheLatestContributor()
{
    // The bridge resolves the shared object but carries no display name of
    // its own, so its missing name must come from the highest-priority
    // contributor (the later configured source) instead of the earliest
    // accumulator position. Contributors are listed from the winner down to
    // the lowest-priority source, each contributing source once.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceC = createCatalog(
        {makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Later B"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-b", "source-a"}));
}

void CatalogIdentityMergeTests::bridgeWinnerKeepsItsOwnMetadata()
{
    // A value the bridge supplies itself stays authoritative
    // even when every absorbed contributor carries its own value for it.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceC = createCatalog(
        {makeStar(
            "c_bridge",
            "Bridge C",
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")},
            {},
            1.0,
            2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Bridge C"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-b", "source-a"}));
}

void CatalogIdentityMergeTests::bridgeInheritsOptionalStarMetadataFromTheLatestContributor()
{
    // Acceptance with optional domain metadata instead of the display name:
    // the bridge owns no astrometry, so its missing fields come from the
    // highest-priority contributor first and are only then completed by lower
    // priority contributors. The later source supplies the proper motion and
    // parallax, the earlier source only fills the radial velocity gap.
    OwnGalaxyCelestialBody earlier =
        makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .stellarParallaxMas = 1.0,
        .radialVelocityKmPerSecond = -1.0,
    };
    OwnGalaxyCelestialBody later = makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0);
    later.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *later.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = -40.0,
        .stellarParallaxMas = 2.0,
    };
    const auto sourceA = createCatalog({earlier}, {});
    const auto sourceB = createCatalog({later}, {});
    const auto sourceC = createCatalog(
        {makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 2.0);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 30.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -40.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, -1.0);
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-b", "source-a"}));
}

void CatalogIdentityMergeTests::reorderingBridgeSourcesChangesInheritedMetadata()
{
    // The same records with sources A and B swapped. The source configured
    // later is the higher-priority contributor, so the inherited name changes
    // from `Later B` to `Earlier A` and the contributor list swaps accordingly.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceC = createCatalog(
        {makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Earlier A"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-a", "source-b"}));
}

void CatalogIdentityMergeTests::bridgeAbsorptionUsesSourcePrecedenceAfterReplacements()
{
    // A replacement runs before the bridge: source-2 replaced source-0's HIP 1
    // survivor, and the replacement's contributor list carries source-0, whose
    // accumulator position is now vacated. The bridge must still inherit from
    // source-2's replacement (the higher-precedence absorbed survivor) and
    // list source-2 before source-1 before source-0. Absorbing in accumulator
    // position order would instead use source-1's name and place source-1
    // before source-2.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto replacement =
        createCatalog({makeStar("c_hip1", "Replacement C", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto bridge = createCatalog(
        {makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), replacement.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "c_hip1"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement C"));
    QCOMPARE(result.sourceIds.front(), std::string("source-3"));
    QCOMPARE(
        result.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogIdentityMergeTests::multiStageBridgesRetainEveryContributorOnce()
{
    // A second bridge absorbs the first bridge's survivor together with one
    // more source. Every contributing source appears exactly once, ordered
    // from the second bridge's source down to the earliest source, and the
    // higher-priority contributor supplies the missing name.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto firstBridge = createCatalog(
        {makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceD =
        createCatalog({makeStar("d_hyg3", "D", {CatalogIdentifier::make("hyg", "3")}, {}, 1.0, 2.0)}, {});
    const auto secondBridge = createCatalog(
        {makeStar(
            "e_bridge", {}, {CatalogIdentifier::make("hd", "2"), CatalogIdentifier::make("hyg", "3")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result =
        composeAll({sourceA.get(), sourceB.get(), firstBridge.get(), sourceD.get(), secondBridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "c_bridge"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "d_hyg3"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "e_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "3"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("D"));
    QCOMPARE(result.sourceIds.front(), std::string("source-4"));
    QCOMPARE(
        result.contributorSourceIds.front(),
        (std::vector<std::string>{"source-4", "source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogIdentityMergeTests::bridgePrefersTheNameOfItsActualSupplyingSource()
{
    // The HIP 1 object is named by the earliest source, and the record that
    // replaces it later carries no name of its own, so the name it holds was
    // supplied by that earliest source. The bridge resolves both objects, and
    // the HD 2 source is the only higher-precedence source that actually
    // supplied a name, so its explicit name wins over the inherited one.
    const auto sourceA =
        createCatalog({makeStar("a_hip_1", "A inherited", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd_2", "B explicit", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceC =
        createCatalog({makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto bridge = createCatalog(
        {makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "c_hip_1"), std::size_t{0});
    QCOMPARE(countBodiesById(result.catalog->bodies(), "b_hd_2"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("B explicit"));
    QCOMPARE(
        result.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogIdentityMergeTests::bridgePrefersTheDeepSkyAxisOfItsActualSupplyingSource()
{
    // Domain metadata follows the same rule: the NGC object carries a major
    // axis of 1 arcminute, the IC object 2 arcminutes, and the record that
    // replaces the NGC survivor supplies no axis of its own. The bridge
    // resolves both designations, so the axis must come from the IC record,
    // the highest-precedence source that actually supplied one.
    const auto sourceA = createCatalog(
        {}, {makeDeepSkyObject("a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0)}
    );
    const auto sourceB =
        createCatalog({}, {makeDeepSkyObject("b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0)});
    const auto sourceC = createCatalog(
        {}, {makeDeepSkyObject("c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt)}
    );
    const auto bridge = createCatalog(
        {},
        {makeDeepSkyObject(
            "d_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(hasIdentifier(*survivor, "ic", "1"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 2.0);
}

void CatalogIdentityMergeTests::explicitReplacementValueOutranksAnInheritedValue()
{
    // The record that replaces the NGC survivor supplies its own major axis,
    // so that explicit value stays authoritative even though the IC object
    // also supplies an axis and the bridge supplies none.
    const auto sourceA = createCatalog(
        {}, {makeDeepSkyObject("a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0)}
    );
    const auto sourceB =
        createCatalog({}, {makeDeepSkyObject("b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0)});
    const auto sourceC = createCatalog(
        {}, {makeDeepSkyObject("c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 3.0)}
    );
    const auto bridge = createCatalog(
        {},
        {makeDeepSkyObject(
            "d_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 3.0);
}

void CatalogIdentityMergeTests::deepSkyValuesResolveIndependentlyBySupplyingSource()
{
    // Two objects carry different deep-sky values for the same body, and both
    // the record that replaces the NGC survivor and the bridge supply no
    // deep-sky metadata of their own. Every field then resolves on its own:
    // the axis and the position angle follow the higher-precedence source,
    // while the minor axis the IC object alone supplies is kept because no
    // other source offers a value for it. Reordering the two data sources
    // swaps the inherited axis and position angle and leaves the minor axis
    // untouched.
    DistantCelestialBody ngcObject =
        makeDeepSkyObject("a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 30.0);
    ngcObject.deepSkyObject->positionAngleDeg = 45.0;
    DistantCelestialBody icObject =
        makeDeepSkyObject("b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 60.0);
    icObject.deepSkyObject->positionAngleDeg = 90.0;
    icObject.deepSkyObject->minorAxisArcmin = 5.0;
    DistantCelestialBody replacement =
        makeDeepSkyObject("c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt);
    replacement.deepSkyObject.reset();
    DistantCelestialBody bridge = makeDeepSkyObject(
        "d_bridge",
        {},
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    bridge.deepSkyObject.reset();

    const auto sourceNgc = createCatalog({}, {ngcObject});
    const auto sourceIc = createCatalog({}, {icObject});
    const auto sourceReplacement = createCatalog({}, {replacement});
    const auto sourceBridge = createCatalog({}, {bridge});

    const CatalogCompositionResult ngcFirst =
        composeAll({sourceNgc.get(), sourceIc.get(), sourceReplacement.get(), sourceBridge.get()});
    QVERIFY(ngcFirst.isSuccess());
    QCOMPARE(ngcFirst.bodyCount, std::size_t{1});
    const BaseCelestialBody* ngcFirstSurvivor = findBodyById(ngcFirst.catalog->bodies(), "d_bridge");
    QVERIFY(ngcFirstSurvivor != nullptr);
    const auto* deepSkyInfo = ngcFirstSurvivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 60.0);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 90.0);
    QVERIFY(deepSkyInfo->minorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->minorAxisArcmin, 5.0);

    const CatalogCompositionResult icFirst =
        composeAll({sourceIc.get(), sourceNgc.get(), sourceReplacement.get(), sourceBridge.get()});
    QVERIFY(icFirst.isSuccess());
    QCOMPARE(icFirst.bodyCount, std::size_t{1});
    const BaseCelestialBody* icFirstSurvivor = findBodyById(icFirst.catalog->bodies(), "d_bridge");
    QVERIFY(icFirstSurvivor != nullptr);
    const auto* reorderedDeepSkyInfo = icFirstSurvivor->deepSkyObjectInfo();
    QVERIFY(reorderedDeepSkyInfo != nullptr);
    QVERIFY(reorderedDeepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*reorderedDeepSkyInfo->majorAxisArcmin, 30.0);
    QVERIFY(reorderedDeepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*reorderedDeepSkyInfo->positionAngleDeg, 45.0);
    QVERIFY(reorderedDeepSkyInfo->minorAxisArcmin.has_value());
    QCOMPARE(*reorderedDeepSkyInfo->minorAxisArcmin, 5.0);
}

void CatalogIdentityMergeTests::absentAndZeroDeepSkyValuesKeepTheirMeaning()
{
    // Only the IC object carries a deep-sky measurement, an explicit zero
    // position angle with an unknown kind. The zero is a value, so the bridge
    // keeps it, while the fields nobody supplied stay absent instead of being
    // invented.
    DistantCelestialBody bareNgc =
        makeDeepSkyObject("a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt);
    bareNgc.deepSkyObject.reset();
    DistantCelestialBody zeroedIc =
        makeDeepSkyObject("b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt);
    zeroedIc.deepSkyObject->kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown;
    zeroedIc.deepSkyObject->positionAngleDeg = 0.0;
    DistantCelestialBody bareBridge = makeDeepSkyObject(
        "c_bridge",
        {},
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    bareBridge.deepSkyObject.reset();

    const auto sourceA = createCatalog({}, {bareNgc});
    const auto sourceB = createCatalog({}, {zeroedIc});
    const auto sourceBridge = createCatalog({}, {bareBridge});

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QCOMPARE(deepSkyInfo->kind, skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 0.0);
    QVERIFY(!deepSkyInfo->majorAxisArcmin.has_value());
    QVERIFY(!deepSkyInfo->minorAxisArcmin.has_value());
}

void CatalogIdentityMergeTests::inheritedAstrometryValuesKeepTheirSupplyingSource()
{
    // The earliest source supplies the parallax of the HIP 1 object, and the
    // record that replaces it carries no astrometry of its own, so the model
    // it holds was supplied by that earliest source. The HD 2 object describes
    // the same direction and supplies a proper motion and its own parallax, so
    // both of its values reach the bridge while the surviving reference
    // position stays the one the winner already describes.
    OwnGalaxyCelestialBody earlier = makeStar("a_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .stellarParallaxMas = 5.0,
    };
    OwnGalaxyCelestialBody later = makeStar("b_hd_2", {}, {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0);
    later.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *later.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = 0.0,
        .stellarParallaxMas = 7.0,
    };
    const auto sourceA = createCatalog({earlier}, {});
    const auto sourceB = createCatalog({later}, {});
    const auto sourceC =
        createCatalog({makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto bridge = createCatalog(
        {makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 7.0);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 30.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, 0.0);
}

void CatalogIdentityMergeTests::inheritedFixedCoordinatesFollowTheirSupplyingSource()
{
    // Neither the replacement nor the bridge carries a position of its own,
    // and the two absorbed objects disagree about where the shared body is.
    // The position supplied by the higher-precedence source wins, and the
    // merge diagnoses the override.
    const auto sourceA =
        createCatalog({makeStar("a_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd_2", {}, {CatalogIdentifier::make("hd", "2")}, {}, 5.0, 6.0)}, {});
    const auto sourceC = createCatalog(
        {makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt)}, {}
    );
    const auto bridge = createCatalog(
        {makeStar(
            "d_bridge",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_hd_2 over conflicting fixed coordinates from d_bridge."
    );
    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 6.0);
}

void CatalogIdentityMergeTests::reorderedReplacementChainChangesTheInheritedName()
{
    // The HIP 1 object is named by the source that first supplies it, and the
    // record that replaces it later carries no name of its own, so the name it
    // holds was supplied by that earliest source. The HD 2 object supplies its
    // own name. The replacement and the bridge never name the body, so the
    // winning name comes from whichever of the two supplying sources ranks
    // higher. Composing the HIP 1 source first lets the explicit HD 2 name
    // win; swapping only the two supplying sources makes the inherited HIP 1
    // name the higher-precedence contribution. The name therefore follows its
    // actual supplying source through both replacement and reordering.
    const auto sourceA =
        createCatalog({makeStar("a_hip_1", "A inherited", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd_2", "B explicit", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto replacement =
        createCatalog({makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto bridge = createCatalog(
        {makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult explicitSourceHigher =
        composeAll({sourceA.get(), sourceB.get(), replacement.get(), bridge.get()});

    QVERIFY(explicitSourceHigher.isSuccess());
    QCOMPARE(explicitSourceHigher.bodyCount, std::size_t{1});
    const BaseCelestialBody* explicitSurvivor = findBodyById(explicitSourceHigher.catalog->bodies(), "d_bridge");
    QVERIFY(explicitSurvivor != nullptr);
    QVERIFY(hasIdentifier(*explicitSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*explicitSurvivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(explicitSurvivor->displayName), QStringLiteral("B explicit"));

    const CatalogCompositionResult inheritedSourceHigher =
        composeAll({sourceB.get(), sourceA.get(), replacement.get(), bridge.get()});

    QVERIFY(inheritedSourceHigher.isSuccess());
    QCOMPARE(inheritedSourceHigher.bodyCount, std::size_t{1});
    const BaseCelestialBody* inheritedSurvivor = findBodyById(inheritedSourceHigher.catalog->bodies(), "d_bridge");
    QVERIFY(inheritedSurvivor != nullptr);
    QVERIFY(hasIdentifier(*inheritedSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*inheritedSurvivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(inheritedSurvivor->displayName), QStringLiteral("A inherited"));
}

void CatalogIdentityMergeTests::retainsReplacedCanonicalIdentityAcrossLaterSources()
{
    // A and B establish that both canonical keys identify the same object
    // because they carry the same HIP cross-identifier. B replaces A, then C
    // arrives carrying A's canonical id without any cross-identifier. The
    // established equivalence must resolve C to the survivor instead of
    // appending a duplicate object.
    const auto sourceA =
        createCatalog({makeStar("original", "Original A", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0)}, {});
    const auto sourceB = createCatalog(
        {makeStar("replacement", "Replacement B", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceC = createCatalog({makeStar("original", "Late C", {}, {}, 1.0, 2.0)}, {});

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "original"), std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "replacement"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(bodies, "original");
    QVERIFY(survivor != nullptr);
    // The later record C wins the public identity and inherits the absorbed
    // cross-identifier.
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Late C"));
    QVERIFY(hasIdentifier(*survivor, "hip", "99"));
    // The replaced canonical id stays authoritative identity data, never a
    // display alias.
    QVERIFY(hasRetainedCanonicalId(*survivor, "replacement"));
    QCOMPARE(survivor->identity.retainedCanonicalIds.size(), std::size_t{1});
    QVERIFY(!hasAlias(survivor->identity.aliases, "replacement"));
    QCOMPARE(result.sourceIds.front(), std::string("source-2"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogIdentityMergeTests::retainsBridgedCanonicalIdentitiesAcrossLaterSources()
{
    // A bridge absorbs every same-kind survivor it resolves, and later
    // replacements keep every absorbed canonical id resolvable, including ids
    // absorbed in an earlier composition stage.
    const auto sourceA = createCatalog({makeStar("a_hip1", {}, {CatalogIdentifier::make("hip", "1")})}, {});
    const auto sourceB = createCatalog({makeStar("b_hd2", {}, {CatalogIdentifier::make("hd", "2")})}, {});
    const auto sourceBridge = createCatalog(
        {makeStar("c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")})}, {}
    );
    const auto sourceReplacement =
        createCatalog({makeStar("d_hd2", "Later D", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceOriginal = createCatalog({makeStar("a_hip1", "Latest A", {}, {}, 1.0, 2.0)}, {});

    const CatalogCompositionResult bridged = composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});
    QVERIFY(bridged.isSuccess());
    QCOMPARE(bridged.bodyCount, std::size_t{1});
    const BaseCelestialBody* bridgeSurvivor = findBodyById(bridged.catalog->bodies(), "c_bridge");
    QVERIFY(bridgeSurvivor != nullptr);
    QVERIFY(hasRetainedCanonicalId(*bridgeSurvivor, "a_hip1"));
    QVERIFY(hasRetainedCanonicalId(*bridgeSurvivor, "b_hd2"));
    QCOMPARE(bridgeSurvivor->identity.retainedCanonicalIds.size(), std::size_t{2});

    const CatalogCompositionResult replaced =
        composeAll({sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get()});
    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* replacementSurvivor = findBodyById(replaced.catalog->bodies(), "d_hd2");
    QVERIFY(replacementSurvivor != nullptr);
    QVERIFY(hasIdentifier(*replacementSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*replacementSurvivor, "hd", "2"));
    QVERIFY(hasRetainedCanonicalId(*replacementSurvivor, "c_bridge"));
    QVERIFY(hasRetainedCanonicalId(*replacementSurvivor, "a_hip1"));
    QVERIFY(hasRetainedCanonicalId(*replacementSurvivor, "b_hd2"));
    QCOMPARE(replacementSurvivor->identity.retainedCanonicalIds.size(), std::size_t{3});

    // A row carrying a canonical id absorbed two stages earlier still resolves
    // to the single survivor instead of appending a fourth object.
    const CatalogCompositionResult lateOriginal =
        composeAll({sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get(), sourceOriginal.get()});
    QVERIFY(lateOriginal.isSuccess());
    QCOMPARE(lateOriginal.bodyCount, std::size_t{1});
    const BaseCelestialBody* finalSurvivor = findBodyById(lateOriginal.catalog->bodies(), "a_hip1");
    QVERIFY(finalSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(finalSurvivor->displayName), QStringLiteral("Latest A"));
    QVERIFY(hasIdentifier(*finalSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*finalSurvivor, "hd", "2"));
    QVERIFY(hasRetainedCanonicalId(*finalSurvivor, "d_hd2"));
    QVERIFY(hasRetainedCanonicalId(*finalSurvivor, "c_bridge"));
    QVERIFY(hasRetainedCanonicalId(*finalSurvivor, "b_hd2"));
}

void CatalogIdentityMergeTests::retainedCanonicalIdentitySurvivesBinaryRoundTripAndRecomposition()
{
    // The established equivalence survives binary
    // serialization, index reconstruction, and reuse of the already-composed
    // snapshot as a composition input.
    const auto sourceA =
        createCatalog({makeStar("original", "Original", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0)}, {});
    const auto sourceB = createCatalog(
        {makeStar("replacement", "Replacement", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0)}, {}
    );

    const CatalogCompositionResult composed = composeInOrder(*sourceA, *sourceB);
    QVERIFY(composed.isSuccess());
    QCOMPARE(composed.bodyCount, std::size_t{1});
    const BaseCelestialBody* composedSurvivor = findBodyById(composed.catalog->bodies(), "replacement");
    QVERIFY(composedSurvivor != nullptr);
    QVERIFY(hasRetainedCanonicalId(*composedSurvivor, "original"));

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(composed.catalog->catalog());
    QVERIFY(!payload.isEmpty());
    const std::unique_ptr<IStarCatalog> restored = skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const BaseCelestialBody* restoredSurvivor = findBodyById(restored->bodies(), "replacement");
    QVERIFY(restoredSurvivor != nullptr);
    QVERIFY(hasIdentifier(*restoredSurvivor, "hip", "99"));
    QVERIFY(hasRetainedCanonicalId(*restoredSurvivor, "original"));

    const auto laterOriginal = createCatalog({makeStar("original", "Late Original", {}, {}, 1.0, 2.0)}, {});

    // The row carrying the earlier canonical id resolves to the restored
    // survivor whether it arrives before or after the restored snapshot, so
    // the rebuilt index resolves retained keys on both sides.
    const CatalogCompositionResult restoredFirst = composeInOrder(*restored, *laterOriginal);
    QVERIFY(restoredFirst.isSuccess());
    QCOMPARE(restoredFirst.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(restoredFirst.catalog->bodies(), "replacement"), std::size_t{0});
    const BaseCelestialBody* lateWinner = findBodyById(restoredFirst.catalog->bodies(), "original");
    QVERIFY(lateWinner != nullptr);
    QCOMPARE(QString::fromStdString(lateWinner->displayName), QStringLiteral("Late Original"));
    QVERIFY(hasIdentifier(*lateWinner, "hip", "99"));
    QVERIFY(hasRetainedCanonicalId(*lateWinner, "replacement"));

    const CatalogCompositionResult restoredLast = composeInOrder(*laterOriginal, *restored);
    QVERIFY(restoredLast.isSuccess());
    QCOMPARE(restoredLast.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(restoredLast.catalog->bodies(), "original"), std::size_t{0});
    const BaseCelestialBody* snapshotWinner = findBodyById(restoredLast.catalog->bodies(), "replacement");
    QVERIFY(snapshotWinner != nullptr);
    QVERIFY(hasIdentifier(*snapshotWinner, "hip", "99"));
    QVERIFY(hasRetainedCanonicalId(*snapshotWinner, "original"));

    // The composed snapshot used directly as a composition input behaves the
    // same way as the serialized one.
    const CatalogCompositionResult composedAsInput = composeInOrder(*laterOriginal, *composed.catalog);
    QVERIFY(composedAsInput.isSuccess());
    QCOMPARE(composedAsInput.bodyCount, std::size_t{1});
    const BaseCelestialBody* composedWinner = findBodyById(composedAsInput.catalog->bodies(), "replacement");
    QVERIFY(composedWinner != nullptr);
    QVERIFY(hasRetainedCanonicalId(*composedWinner, "original"));
}

void CatalogIdentityMergeTests::combinedChainKeepsIdentityMetadataAndProvenanceAcrossSnapshotRecomposition()
{
    // A three-source chain whose bridge resolves two name owners, followed by a
    // replacement that carries the bridge's second key. The bridge inherits its
    // missing display name from the highest-precedence contributor, and the
    // replacement absorbs the bridge while keeping every earlier canonical id
    // resolvable. Composing the earlier chain through its binary snapshot must
    // reach the same survivor identity, metadata, and contributor accounting as
    // composing all four sources in one pass.
    const auto sourceA =
        createCatalog({makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {});
    const auto sourceB =
        createCatalog({makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});
    const auto sourceBridge = createCatalog(
        {makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceReplacement =
        createCatalog({makeStar("d_hd2", "Latest D", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {});

    const CatalogCompositionResult direct =
        composeAll({sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get()});
    QVERIFY(direct.isSuccess());
    QCOMPARE(direct.bodyCount, std::size_t{1});
    const BaseCelestialBody* directSurvivor = findBodyById(direct.catalog->bodies(), "d_hd2");
    QVERIFY(directSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(directSurvivor->displayName), QStringLiteral("Latest D"));
    QVERIFY(hasIdentifier(*directSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*directSurvivor, "hd", "2"));
    QVERIFY(hasRetainedCanonicalId(*directSurvivor, "c_bridge"));
    QVERIFY(hasRetainedCanonicalId(*directSurvivor, "a_hip1"));
    QVERIFY(hasRetainedCanonicalId(*directSurvivor, "b_hd2"));
    QCOMPARE(
        direct.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );

    // The earlier chain on its own: one bridge survivor that inherited its
    // missing name from source B, the highest-precedence contributor, and that
    // accounts for all three contributors once.
    const CatalogCompositionResult bridgeChain = composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});
    QVERIFY(bridgeChain.isSuccess());
    QCOMPARE(bridgeChain.bodyCount, std::size_t{1});
    const BaseCelestialBody* bridgeSurvivor = findBodyById(bridgeChain.catalog->bodies(), "c_bridge");
    QVERIFY(bridgeSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(bridgeSurvivor->displayName), QStringLiteral("Later B"));
    QVERIFY(hasIdentifier(*bridgeSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*bridgeSurvivor, "hd", "2"));
    QVERIFY(hasRetainedCanonicalId(*bridgeSurvivor, "a_hip1"));
    QVERIFY(hasRetainedCanonicalId(*bridgeSurvivor, "b_hd2"));
    QCOMPARE(bridgeSurvivor->identity.retainedCanonicalIds.size(), std::size_t{2});
    QCOMPARE(bridgeChain.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(bridgeChain.catalog->catalog());
    QVERIFY(!payload.isEmpty());
    const std::unique_ptr<IStarCatalog> restored = skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    // Recomposing the restored snapshot with the same later replacement must
    // resolve to one survivor instead of appending the replacement as a second
    // object, so identity, metadata, and retained canonical ids stay equal to
    // the single-pass composition.
    const CatalogCompositionResult snapshotChain = composeAll({restored.get(), sourceReplacement.get()});
    QVERIFY(snapshotChain.isSuccess());
    QCOMPARE(snapshotChain.bodyCount, std::size_t{1});
    QCOMPARE(countBodiesById(snapshotChain.catalog->bodies(), "d_hd2"), std::size_t{1});
    const BaseCelestialBody* snapshotSurvivor = findBodyById(snapshotChain.catalog->bodies(), "d_hd2");
    QVERIFY(snapshotSurvivor != nullptr);
    QCOMPARE(snapshotSurvivor->id, directSurvivor->id);
    QCOMPARE(snapshotSurvivor->displayName, directSurvivor->displayName);
    QCOMPARE(snapshotSurvivor->kind, directSurvivor->kind);
    QCOMPARE(sortedIdentifierKeys(*snapshotSurvivor), sortedIdentifierKeys(*directSurvivor));
    QCOMPARE(sortedRetainedCanonicalIds(*snapshotSurvivor), sortedRetainedCanonicalIds(*directSurvivor));
    QVERIFY(snapshotSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(
        snapshotSurvivor->fixedEquatorialValue()->rightAscensionHours,
        directSurvivor->fixedEquatorialValue()->rightAscensionHours
    );
    QCOMPARE(
        snapshotSurvivor->fixedEquatorialValue()->declinationDeg, directSurvivor->fixedEquatorialValue()->declinationDeg
    );

    // Provenance stays a complete accounting at each boundary: the chain names
    // its three contributors once, and the recomposition names the snapshot and
    // the later replacement once each.
    QCOMPARE(snapshotChain.sourceIds.front(), std::string("source-1"));
    QCOMPARE(snapshotChain.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));

    // The canonical id that survived the replacement inside the snapshot still
    // resolves when a row carrying it arrives after serialization.
    const auto lateOriginal = createCatalog({makeStar("a_hip1", "Late A", {}, {}, 1.0, 2.0)}, {});
    const CatalogCompositionResult lateResult = composeAll({restored.get(), lateOriginal.get()});
    QVERIFY(lateResult.isSuccess());
    QCOMPARE(lateResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* lateSurvivor = findBodyById(lateResult.catalog->bodies(), "a_hip1");
    QVERIFY(lateSurvivor != nullptr);
    QVERIFY(hasIdentifier(*lateSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*lateSurvivor, "hd", "2"));
    QVERIFY(hasRetainedCanonicalId(*lateSurvivor, "c_bridge"));
    QVERIFY(hasRetainedCanonicalId(*lateSurvivor, "b_hd2"));
}

QTEST_APPLESS_MAIN(CatalogIdentityMergeTests)

#include "CatalogIdentityMergeTests.moc"
