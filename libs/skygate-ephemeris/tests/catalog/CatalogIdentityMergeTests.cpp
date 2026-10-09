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
#include <cmath>
#include <cstddef>
#include <limits>
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
    std::optional<double> declinationDeg = 2.0,
    double visualMagnitude = std::numeric_limits<double>::quiet_NaN()
)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::Star;
    body.visualMagnitude = visualMagnitude;
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
    std::optional<double> majorAxisArcmin = std::nullopt,
    double visualMagnitude = std::numeric_limits<double>::quiet_NaN()
)
{
    DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
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

// Gives a star row a reference position and reference epoch that agree with
// its fixed position, so both coordinate representations describe one model.
[[nodiscard]] OwnGalaxyCelestialBody withReferenceModel(OwnGalaxyCelestialBody body)
{
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *body.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    return body;
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
    void bridgesWithinOneSourceKeepFirstRowName();
    void bridgesWithinOneSourceKeepFirstRowDeepSkyValues();
    void withinSourceBridgeFieldsResolveByEarliestSupplyingRow();
    void secondWithinSourceBridgeKeepsEarliestRowValues();
    void sameSourcePassKeepsEarliestRowNameAcrossALocalBridge();
    void sameSourcePassKeepsEarliestRowDeepSkyValues();
    void sameSourcePassMatchResolvesEachFieldByItsSupplyingRow();
    void secondBridgeSurvivorKeepsRowOriginsUnderLaterReplacement();
    void withinSourceBridgeSurvivorKeepsRowOrderAcrossCrossSourceBridge();
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
    void bridgesWithinOneSourceResolveMagnitudeBySupplyingRow();
    void zeroAndNegativeMagnitudesKeepSupplyingRowPrecedence();
    void bridgedDeepSkyMagnitudesFillOnlyMissingValues();
    void replacementMagnitudeKeepsSourcePrecedence();
    void earliestRowModelWinsAcrossALocalBridge();
    void ordinaryDuplicateRowsKeepTheFirstRowsCoherentModel();
    void reversedRowsAndALocalBridgeKeepTheFirstRowsModel();
    void referenceOnlyAstrometryAnchorsTheEarliestRowModel();
    void repeatedBridgesKeepTheEarliestRowModel();
    void epochCompatibilityDecidesTheOptionalFieldsOfTheEarliestModel();
    void laterSourceModelReplacesAnEarlierBridgedModel();
    void secondPassKeepsTheEarliestRowModelAcrossALateLocalBridge();
    void adoptsAContradictoryDonorsFixedPositionWithoutItsAstrometry();
    void localBridgeAndLaterReplacementKeepNameMagnitudeAndModel();
    void samePassDuplicateKeepsMagnitudeInheritedFromAnEarlierSource();
    void repeatedBridgesKeepTheEarliestModelUnderALaterReplacement();
    void earliestFixedRowKeepsTheBridgesMeasuredAstrometry();
    void duplicateBridgesKeepTheEarliestRowsModelAndMeasurements();
    void adoptedAstrometrySurvivesWithoutAnIndependentModel();
    void earliestFixedRowRejectsTheBridgesConflictingAstrometry();
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

void CatalogIdentityMergeTests::bridgesWithinOneSourceKeepFirstRowName()
{
    // One source carries three rows: a HIP-only row, an HD-only row, and a
    // bridging row that names both identifiers. The bridge collapses the rows
    // into one object, but the earlier rows keep their value precedence: the
    // first row named the object, so its name survives the bridge.
    const auto source = createCatalog(
        {
            makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}),
            makeStar("b_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}),
            makeStar(
                "c_bridge", "Last bridge", {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}
            ),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_hip_17"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "b_hd_27"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(bodies, "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hd_27"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::bridgesWithinOneSourceKeepFirstRowDeepSkyValues()
{
    // The NGC row supplies the major axis and an explicit zero position angle,
    // the IC row supplies its own measurements, and the bridging row supplies
    // further values of its own. The first row keeps its fields, the explicit
    // zero stays a real value, and the fields only later rows supply fill the
    // gaps.
    DistantCelestialBody ngcRow =
        makeDeepSkyObject("a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0);
    ngcRow.deepSkyObject->positionAngleDeg = 0.0;
    DistantCelestialBody icRow =
        makeDeepSkyObject("b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0);
    icRow.deepSkyObject->positionAngleDeg = 90.0;
    icRow.deepSkyObject->minorAxisArcmin = 5.0;
    DistantCelestialBody bridge = makeDeepSkyObject(
        "c_bridge", {}, {}, {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 3.0
    );
    bridge.deepSkyObject->positionAngleDeg = 180.0;

    const auto source = createCatalog({}, {ngcRow, icRow, bridge});
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(hasIdentifier(*survivor, "ic", "1"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 1.0);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 0.0);
    QVERIFY(deepSkyInfo->minorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->minorAxisArcmin, 5.0);
}

void CatalogIdentityMergeTests::withinSourceBridgeFieldsResolveByEarliestSupplyingRow()
{
    // The first row names the object and supplies the major axis, the second
    // row supplies the position angle and its own values, and the bridge
    // supplies both fields itself. Each field of the survivor must come from
    // its earliest supplying row; reversing the two rows reverses the result.
    DistantCelestialBody ngcRow =
        makeDeepSkyObject("a_ngc_224", "Earlier name", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0);
    DistantCelestialBody icRow =
        makeDeepSkyObject("b_ic_1", "Later name", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0);
    icRow.deepSkyObject->positionAngleDeg = 45.0;
    DistantCelestialBody bridge = makeDeepSkyObject(
        "c_bridge",
        "Bridge name",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        3.0
    );

    const auto forwardSource = createCatalog({}, {ngcRow, icRow, bridge});
    QVERIFY(forwardSource != nullptr);
    const CatalogCompositionResult forward = composePrimary(*forwardSource);
    QVERIFY(forward.isSuccess());
    QCOMPARE(forward.bodyCount, std::size_t{1});
    const BaseCelestialBody* forwardSurvivor = findBodyById(forward.catalog->bodies(), "c_bridge");
    QVERIFY(forwardSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(forwardSurvivor->displayName), QStringLiteral("Earlier name"));
    const auto* forwardInfo = forwardSurvivor->deepSkyObjectInfo();
    QVERIFY(forwardInfo != nullptr);
    QVERIFY(forwardInfo->majorAxisArcmin.has_value());
    QCOMPARE(*forwardInfo->majorAxisArcmin, 1.0);
    QVERIFY(forwardInfo->positionAngleDeg.has_value());
    QCOMPARE(*forwardInfo->positionAngleDeg, 45.0);

    const auto reversedSource = createCatalog({}, {icRow, ngcRow, bridge});
    QVERIFY(reversedSource != nullptr);
    const CatalogCompositionResult reversed = composePrimary(*reversedSource);
    QVERIFY(reversed.isSuccess());
    QCOMPARE(reversed.bodyCount, std::size_t{1});
    const BaseCelestialBody* reversedSurvivor = findBodyById(reversed.catalog->bodies(), "c_bridge");
    QVERIFY(reversedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(reversedSurvivor->displayName), QStringLiteral("Later name"));
    const auto* reversedInfo = reversedSurvivor->deepSkyObjectInfo();
    QVERIFY(reversedInfo != nullptr);
    QVERIFY(reversedInfo->majorAxisArcmin.has_value());
    QCOMPARE(*reversedInfo->majorAxisArcmin, 2.0);
    QVERIFY(reversedInfo->positionAngleDeg.has_value());
    QCOMPARE(*reversedInfo->positionAngleDeg, 45.0);
}

void CatalogIdentityMergeTests::secondWithinSourceBridgeKeepsEarliestRowValues()
{
    // The first bridge absorbs the survivors of rows one and three and is
    // appended at a position later than row two's survivor. The second bridge
    // absorbs that survivor together with row two's own, so accumulator
    // position order would promote row two over row one. Row order decides:
    // the name the first row supplied survives both bridges.
    const auto source = createCatalog(
        {
            makeStar("a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}),
            makeStar("b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}),
            makeStar("c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}),
            makeStar(
                "d_bridge_13", "Bridge D", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")}
            ),
            makeStar(
                "e_bridge_all",
                "Last bridge",
                {CatalogIdentifier::make("hyg", "2"), CatalogIdentifier::make("hip", "1")}
            ),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "e_bridge_all");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First A"));
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_hip_1"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hyg_2"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "c_hip_3"));
}

void CatalogIdentityMergeTests::sameSourcePassKeepsEarliestRowNameAcrossALocalBridge()
{
    // Source A links HIP 1 and HYG 2. Source B names the object in its first
    // row, names HYG 2 in its second row, and bridges HIP 1 with HIP 3 in its
    // last row. The bridge absorbs the first row and is visited after the
    // second row replaced A's survivor, so the first row's value arrives out
    // of encounter order; its recorded row origin still wins.
    const auto sourceA = createCatalog(
        {makeStar("a_linked", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")})}, {}
    );
    const auto sourceB = createCatalog(
        {
            makeStar("b_hip_1", "First", {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0),
            makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}),
            makeStar("b_hip_3", "Third", {CatalogIdentifier::make("hip", "3")}),
            makeStar(
                "b_bridge", "Last bridge", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")}
            ),
        },
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_hip_1 over conflicting fixed coordinates from b_bridge."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_bridge over conflicting fixed coordinates from b_hip_3."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_bridge over conflicting fixed coordinates from b_hyg_2."
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 20.0);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hip_1"));

    // The same rows without the local bridge already keep the first name, so
    // bridging must not change which value wins.
    const auto controlB = createCatalog(
        {
            makeStar("c_hip_1", "First", {CatalogIdentifier::make("hip", "1")}),
            makeStar("c_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}),
        },
        {}
    );
    QVERIFY(controlB != nullptr);

    const CatalogCompositionResult control = composeAll({sourceA.get(), controlB.get()});

    QVERIFY(control.isSuccess());
    QCOMPARE(control.bodyCount, std::size_t{1});
    const BaseCelestialBody* controlSurvivor = findBodyById(control.catalog->bodies(), "c_hip_1");
    QVERIFY(controlSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(controlSurvivor->displayName), QStringLiteral("First"));
}

void CatalogIdentityMergeTests::sameSourcePassKeepsEarliestRowDeepSkyValues()
{
    // Source A links NGC 224 with IC 1. Source B measures NGC 224 in its first
    // row and NGC 225 in its third row; its bridge row joins both, so the
    // bridge absorbs the first row's name and major axis. The second row's
    // IC 1 survivor replaces A's object first and the later bridge survivor
    // matches it, so the first row's name and axis still win.
    const auto sourceA = createCatalog(
        {},
        {makeDeepSkyObject(
            "a_linked_dso",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );
    const auto sourceB = createCatalog(
        {},
        {
            makeDeepSkyObject("b_ngc_224", "First DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0),
            makeDeepSkyObject("b_ic_1", "Second DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0),
            makeDeepSkyObject("b_ngc_225", "Third DSO", {}, {CatalogIdentifier::make("ngc", "225")}, 1.0, 2.0, 3.0),
            makeDeepSkyObject(
                "b_bridge_dso",
                "Bridge DSO",
                {},
                {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ngc", "225")},
                1.0,
                2.0,
                std::nullopt
            ),
        }
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_ic_1");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First DSO"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 1.0);
    QVERIFY(hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(hasIdentifier(*survivor, "ic", "1"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked_dso"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge_dso"));
}

void CatalogIdentityMergeTests::sameSourcePassMatchResolvesEachFieldByItsSupplyingRow()
{
    // The rows are reversed: HYG 2 names the object in the first row, while
    // HIP 1 supplies its own name and coordinates in the second row, which the
    // bridge absorbs. The name the first row supplied is kept, and the
    // coordinates the winner is missing are filled from the second row, so the
    // same-pass match fills missing fields and each field follows its own
    // supplying row.
    const auto sourceA = createCatalog(
        {makeStar(
            "a_linked",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );
    const auto sourceB = createCatalog(
        {
            makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt),
            makeStar("b_hip_1", "First", {CatalogIdentifier::make("hip", "1")}, {}, 22.0, 2.5),
            makeStar("b_hip_3", "Third", {CatalogIdentifier::make("hip", "3")}, {}, 23.0, 2.0),
            makeStar(
                "b_bridge",
                "Last bridge",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                std::nullopt,
                std::nullopt
            ),
        },
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Second"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 22.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.5);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge"));
}

void CatalogIdentityMergeTests::secondBridgeSurvivorKeepsRowOriginsUnderLaterReplacement()
{
    // Source A links HIP 1 and HYG 2, and source B bridges its own rows twice
    // inside its dedup pass, the second bridge absorbing the HYG 2 survivor.
    // Source C then replaces the resulting survivor across sources: the
    // replacement's own name wins by source precedence, the coordinates it
    // does not supply stay with the earliest row that supplied them, and the
    // canonical identities of every bridged and replaced row stay resolvable.
    const auto sourceA = createCatalog(
        {makeStar("a_linked", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")})}, {}
    );
    const auto sourceB = createCatalog(
        {
            makeStar("b_hip_1", "First B", {CatalogIdentifier::make("hip", "1")}, {}, 3.0, 2.0),
            makeStar("b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 4.0, 2.0),
            makeStar("b_hip_3", "Third B", {CatalogIdentifier::make("hip", "3")}, {}, 5.0, 2.0),
            makeStar(
                "b_bridge_13",
                "Bridge B",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                6.0,
                2.0
            ),
            makeStar(
                "b_bridge_all",
                "Last B",
                {CatalogIdentifier::make("hyg", "2"), CatalogIdentifier::make("hip", "1")},
                {},
                7.0,
                2.0
            ),
        },
        {}
    );
    const auto sourceC = createCatalog(
        {makeStar("c_replace", "Replacement", {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt)},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 3.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge_13"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge_all"));
}

void CatalogIdentityMergeTests::withinSourceBridgeSurvivorKeepsRowOrderAcrossCrossSourceBridge()
{
    // The first source deduplicates three rows into one bridged survivor whose
    // name still comes from its first row. A later source bridges that
    // survivor with its own object, so a higher source rank supplies missing
    // fields while the deduplicated survivor keeps the row order of the source
    // that produced it, and the canonical identities of every absorbed row
    // stay resolvable.
    const auto sourceA = createCatalog(
        {
            makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}),
            makeStar("a_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}),
            makeStar(
                "a_bridge", "Last bridge", {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}
            ),
        },
        {}
    );
    const auto sourceB = createCatalog({makeStar("b_hyg_5", {}, {CatalogIdentifier::make("hyg", "5")})}, {});
    const auto sourceC = createCatalog(
        {makeStar("c_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hyg", "5")})}, {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "5"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_bridge"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_hd_27"));
    QCOMPARE(result.sourceIds.front(), std::string("source-2"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);

    // The same deduplicated source with a replacement instead of a bridge: the
    // higher-ranked source's own value stays authoritative, and the absorbed
    // survivor keeps all of its identities resolvable.
    const auto replacement =
        createCatalog({makeStar("d_hip_17", "Replaced", {CatalogIdentifier::make("hip", "17")})}, {});
    QVERIFY(replacement != nullptr);
    const CatalogCompositionResult replaced = composeAll({sourceA.get(), replacement.get()});
    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* replacedSurvivor = findBodyById(replaced.catalog->bodies(), "d_hip_17");
    QVERIFY(replacedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(replacedSurvivor->displayName), QStringLiteral("Replaced"));
    QVERIFY(hasIdentifier(*replacedSurvivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*replacedSurvivor, "a_bridge"));
    QVERIFY(hasRetainedCanonicalId(*replacedSurvivor, "a_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*replacedSurvivor, "a_hd_27"));
    QCOMPARE(replaced.sourceIds.front(), std::string("source-1"));
    QCOMPARE(replaced.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
    QVERIFY(replacedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(replacedSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(replacedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
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

void CatalogIdentityMergeTests::bridgesWithinOneSourceResolveMagnitudeBySupplyingRow()
{
    // The bridging row carries the largest magnitude of the fixture, but the
    // magnitude is enriched metadata like every other field: the earliest row
    // that supplied one keeps it, so the bridge cannot change the composed
    // value. Identity and name keep their own policies on the same rows.
    const auto source = createCatalog(
        {
            makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.0),
            makeStar("b_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0, 2.0),
            makeStar(
                "c_bridge",
                "Last bridge",
                {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
                {},
                1.0,
                2.0,
                3.0
            ),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(countBodiesById(bodies, "a_hip_17"), std::size_t{0});
    QCOMPARE(countBodiesById(bodies, "b_hd_27"), std::size_t{0});

    const BaseCelestialBody* survivor = findBodyById(bodies, "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QCOMPARE(survivor->visualMagnitude, 1.0);
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hd_27"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::zeroAndNegativeMagnitudesKeepSupplyingRowPrecedence()
{
    // The first row supplies an explicit zero magnitude, the second row a
    // negative one, and both later bridge rows magnitudes of their own. Zero is
    // a value, so the earliest row keeps the magnitude through both local
    // bridges. Giving the two earliest rows the opposite magnitudes moves the
    // winner with them, so the result follows row precedence instead of a
    // preference for one sign.
    const auto zeroFirst = createCatalog(
        {
            makeStar("a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 0.0),
            makeStar("b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0, -1.5),
            makeStar("c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 2.0, 2.0),
            makeStar(
                "d_bridge_13",
                "Bridge D",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                1.0,
                2.0,
                -0.5
            ),
            makeStar(
                "e_bridge_all",
                "Last bridge",
                {CatalogIdentifier::make("hyg", "2"), CatalogIdentifier::make("hip", "1")},
                {},
                1.0,
                2.0,
                3.0
            ),
        },
        {}
    );
    QVERIFY(zeroFirst != nullptr);

    const CatalogCompositionResult zeroFirstResult = composePrimary(*zeroFirst);

    QVERIFY(zeroFirstResult.isSuccess());
    QCOMPARE(zeroFirstResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* zeroFirstSurvivor = findBodyById(zeroFirstResult.catalog->bodies(), "e_bridge_all");
    QVERIFY(zeroFirstSurvivor != nullptr);
    QCOMPARE(zeroFirstSurvivor->visualMagnitude, 0.0);
    QCOMPARE(QString::fromStdString(zeroFirstSurvivor->displayName), QStringLiteral("First A"));
    QVERIFY(hasIdentifier(*zeroFirstSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*zeroFirstSurvivor, "hip", "3"));
    QVERIFY(hasIdentifier(*zeroFirstSurvivor, "hyg", "2"));
    QVERIFY(hasRetainedCanonicalId(*zeroFirstSurvivor, "a_hip_1"));
    QVERIFY(hasRetainedCanonicalId(*zeroFirstSurvivor, "b_hyg_2"));
    QVERIFY(hasRetainedCanonicalId(*zeroFirstSurvivor, "c_hip_3"));

    const auto negativeFirst = createCatalog(
        {
            makeStar("a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, -1.5),
            makeStar("b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0, 0.0),
            makeStar("c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 2.0, 2.0),
            makeStar(
                "d_bridge_13",
                "Bridge D",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                1.0,
                2.0,
                -0.5
            ),
            makeStar(
                "e_bridge_all",
                "Last bridge",
                {CatalogIdentifier::make("hyg", "2"), CatalogIdentifier::make("hip", "1")},
                {},
                1.0,
                2.0,
                3.0
            ),
        },
        {}
    );
    QVERIFY(negativeFirst != nullptr);

    const CatalogCompositionResult negativeFirstResult = composePrimary(*negativeFirst);

    QVERIFY(negativeFirstResult.isSuccess());
    QCOMPARE(negativeFirstResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* negativeFirstSurvivor =
        findBodyById(negativeFirstResult.catalog->bodies(), "e_bridge_all");
    QVERIFY(negativeFirstSurvivor != nullptr);
    QCOMPARE(negativeFirstSurvivor->visualMagnitude, -1.5);
    QCOMPARE(QString::fromStdString(negativeFirstSurvivor->displayName), QStringLiteral("First A"));
    QVERIFY(hasRetainedCanonicalId(*negativeFirstSurvivor, "a_hip_1"));
    QVERIFY(hasRetainedCanonicalId(*negativeFirstSurvivor, "b_hyg_2"));
    QVERIFY(hasRetainedCanonicalId(*negativeFirstSurvivor, "c_hip_3"));
}

void CatalogIdentityMergeTests::bridgedDeepSkyMagnitudesFillOnlyMissingValues()
{
    // A deep-sky row the parser could not measure carries a NaN magnitude, so
    // it supplies no value and the bridge row's own value stays the starting
    // point. The first row that does supply a magnitude therefore fills the
    // gap, while a later row never overrides an earlier supplying row.
    DistantCelestialBody unmeasuredNgc = makeDeepSkyObject(
        "a_ngc_224", "First DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    DistantCelestialBody measuredIc =
        makeDeepSkyObject("b_ic_1", "Second DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt);
    measuredIc.visualMagnitude = 4.5;
    DistantCelestialBody unmeasuredBridge = makeDeepSkyObject(
        "c_bridge",
        "Bridge DSO",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );

    const auto gapFillingSource = createCatalog({}, {unmeasuredNgc, measuredIc, unmeasuredBridge});
    QVERIFY(gapFillingSource != nullptr);

    const CatalogCompositionResult gapFilling = composePrimary(*gapFillingSource);

    QVERIFY(gapFilling.isSuccess());
    QCOMPARE(gapFilling.bodyCount, std::size_t{1});
    QCOMPARE(gapFilling.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* gapFillingSurvivor = findBodyById(gapFilling.catalog->bodies(), "c_bridge");
    QVERIFY(gapFillingSurvivor != nullptr);
    QCOMPARE(gapFillingSurvivor->visualMagnitude, 4.5);
    QCOMPARE(QString::fromStdString(gapFillingSurvivor->displayName), QStringLiteral("First DSO"));
    QVERIFY(hasIdentifier(*gapFillingSurvivor, "ngc", "224"));
    QVERIFY(hasIdentifier(*gapFillingSurvivor, "ic", "1"));
    QVERIFY(hasRetainedCanonicalId(*gapFillingSurvivor, "a_ngc_224"));
    QVERIFY(hasRetainedCanonicalId(*gapFillingSurvivor, "b_ic_1"));

    // The earliest row supplies a magnitude of its own and the bridge measures
    // the same object differently, so the bridge must not override it. The
    // middle row supplies no magnitude and stays absent instead of becoming a
    // zero.
    DistantCelestialBody measuredNgc = makeDeepSkyObject(
        "d_ngc_224", "Measured DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    measuredNgc.visualMagnitude = 2.5;
    DistantCelestialBody unmeasuredIc =
        makeDeepSkyObject("e_ic_1", "Unmeasured DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt);
    DistantCelestialBody measuredBridge = makeDeepSkyObject(
        "f_bridge",
        "Bridge DSO",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    measuredBridge.visualMagnitude = 7.0;

    const auto measuredSource = createCatalog({}, {measuredNgc, unmeasuredIc, measuredBridge});
    QVERIFY(measuredSource != nullptr);

    const CatalogCompositionResult measured = composePrimary(*measuredSource);

    QVERIFY(measured.isSuccess());
    QCOMPARE(measured.bodyCount, std::size_t{1});
    const BaseCelestialBody* measuredSurvivor = findBodyById(measured.catalog->bodies(), "f_bridge");
    QVERIFY(measuredSurvivor != nullptr);
    QCOMPARE(measuredSurvivor->visualMagnitude, 2.5);
    QCOMPARE(QString::fromStdString(measuredSurvivor->displayName), QStringLiteral("Measured DSO"));
    QVERIFY(hasRetainedCanonicalId(*measuredSurvivor, "d_ngc_224"));
    QVERIFY(hasRetainedCanonicalId(*measuredSurvivor, "e_ic_1"));

    // No row of the fixture supplies a magnitude, so the survivor carries
    // none: an unmeasured body never gains a magnitude from a bridge.
    DistantCelestialBody bareNgc =
        makeDeepSkyObject("g_ngc_224", "Bare DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt);
    DistantCelestialBody bareIc =
        makeDeepSkyObject("h_ic_1", "Bare IC", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt);
    DistantCelestialBody bareBridge = makeDeepSkyObject(
        "i_bridge",
        "Bare bridge",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );

    const auto bareSource = createCatalog({}, {bareNgc, bareIc, bareBridge});
    QVERIFY(bareSource != nullptr);

    const CatalogCompositionResult bare = composePrimary(*bareSource);

    QVERIFY(bare.isSuccess());
    QCOMPARE(bare.bodyCount, std::size_t{1});
    const BaseCelestialBody* bareSurvivor = findBodyById(bare.catalog->bodies(), "i_bridge");
    QVERIFY(bareSurvivor != nullptr);
    QVERIFY(std::isnan(bareSurvivor->visualMagnitude));
    QCOMPARE(QString::fromStdString(bareSurvivor->displayName), QStringLiteral("Bare DSO"));
    QVERIFY(hasIdentifier(*bareSurvivor, "ngc", "224"));
    QVERIFY(hasIdentifier(*bareSurvivor, "ic", "1"));
}

void CatalogIdentityMergeTests::replacementMagnitudeKeepsSourcePrecedence()
{
    // A replacing source outranks the source it replaces, so its explicit
    // magnitude wins over the replaced survivor's earlier one, while a
    // replacement that supplies no magnitude inherits the replaced value
    // instead of erasing it.
    const auto sourceA =
        createCatalog({makeStar("a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 1.0)}, {});
    const auto explicitReplacement = createCatalog(
        {makeStar("b_hip_1", "Replacement B", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 2.0)}, {}
    );
    const auto silentReplacement =
        createCatalog({makeStar("c_hip_1", "Late C", {CatalogIdentifier::make("hip", "1")})}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(explicitReplacement != nullptr);
    QVERIFY(silentReplacement != nullptr);

    const CatalogCompositionResult explicitResult = composeAll({sourceA.get(), explicitReplacement.get()});

    QVERIFY(explicitResult.isSuccess());
    QCOMPARE(explicitResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* explicitSurvivor = findBodyById(explicitResult.catalog->bodies(), "b_hip_1");
    QVERIFY(explicitSurvivor != nullptr);
    QCOMPARE(explicitSurvivor->visualMagnitude, 2.0);
    QCOMPARE(QString::fromStdString(explicitSurvivor->displayName), QStringLiteral("Replacement B"));
    QVERIFY(hasIdentifier(*explicitSurvivor, "hip", "1"));
    QVERIFY(hasRetainedCanonicalId(*explicitSurvivor, "a_hip_1"));

    const CatalogCompositionResult inheritedResult = composeAll({sourceA.get(), silentReplacement.get()});

    QVERIFY(inheritedResult.isSuccess());
    QCOMPARE(inheritedResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* inheritedSurvivor = findBodyById(inheritedResult.catalog->bodies(), "c_hip_1");
    QVERIFY(inheritedSurvivor != nullptr);
    QCOMPARE(inheritedSurvivor->visualMagnitude, 1.0);
    QCOMPARE(QString::fromStdString(inheritedSurvivor->displayName), QStringLiteral("Late C"));
    QVERIFY(hasIdentifier(*inheritedSurvivor, "hip", "1"));
    QVERIFY(hasRetainedCanonicalId(*inheritedSurvivor, "a_hip_1"));

    // The same policies hold through a local bridge chain: the bridged source
    // row that supplied the magnitude first decides its value, and the later
    // replacing source without a magnitude of its own inherits it instead of
    // falling back to the lower-ranked source's earlier value. Name, identity,
    // and magnitude follow their own policies on the same rows.
    const auto firstSource =
        createCatalog({makeStar("a_hip_17", "First A", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.5)}, {});
    const auto bridgedSource = createCatalog(
        {
            makeStar("b_hip_17", "First B", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 2.5),
            makeStar("b_hd_27", "Second B", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0, 3.5),
            makeStar(
                "b_bridge",
                "Bridge B",
                {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
                {},
                1.0,
                2.0,
                9.0
            ),
        },
        {}
    );
    const auto replacementWithoutMagnitude =
        createCatalog({makeStar("c_hip_17", "Replacement C", {CatalogIdentifier::make("hip", "17")})}, {});
    QVERIFY(firstSource != nullptr);
    QVERIFY(bridgedSource != nullptr);
    QVERIFY(replacementWithoutMagnitude != nullptr);

    const CatalogCompositionResult chain =
        composeAll({firstSource.get(), bridgedSource.get(), replacementWithoutMagnitude.get()});

    QVERIFY(chain.isSuccess());
    QCOMPARE(chain.bodyCount, std::size_t{1});
    const BaseCelestialBody* chainSurvivor = findBodyById(chain.catalog->bodies(), "c_hip_17");
    QVERIFY(chainSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(chainSurvivor->displayName), QStringLiteral("Replacement C"));
    QCOMPARE(chainSurvivor->visualMagnitude, 2.5);
    QVERIFY(hasIdentifier(*chainSurvivor, "hip", "17"));
    QVERIFY(hasIdentifier(*chainSurvivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*chainSurvivor, "a_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*chainSurvivor, "b_hip_17"));
    QVERIFY(hasRetainedCanonicalId(*chainSurvivor, "b_hd_27"));
    QVERIFY(hasRetainedCanonicalId(*chainSurvivor, "b_bridge"));
    QCOMPARE(chain.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogIdentityMergeTests::earliestRowModelWinsAcrossALocalBridge()
{
    // HIP 17 and HD 27 each describe the object at RA 1 hour with a matching
    // reference position at the same epoch, while the bridge describes RA 5
    // hours in both representations. The first row's coherent model must win
    // as a whole: the bridge supplies the public canonical id, not the model.
    OwnGalaxyCelestialBody hipRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    hipRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hipRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody hdRow = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    hdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hdRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    const auto source = createCatalog({hipRow, hdRow, bridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "bridge_last."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
}

void CatalogIdentityMergeTests::ordinaryDuplicateRowsKeepTheFirstRowsCoherentModel()
{
    // Two rows of one source place the object at RA 1 and RA 5 with internally
    // consistent models. The first row is authoritative within its source, so
    // a duplicate never replaces its model.
    OwnGalaxyCelestialBody firstRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody duplicateRow =
        makeStar("hip_17_copy", "Second", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0);
    duplicateRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *duplicateRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    const auto source = createCatalog({firstRow, duplicateRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "hip_17_copy."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "hip_17_copy."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "hip_17");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
}

void CatalogIdentityMergeTests::reversedRowsAndALocalBridgeKeepTheFirstRowsModel()
{
    // The first row declares RA 5 and the second row RA 1, and the bridge
    // declares RA 5 while absorbing both. The first row's model still wins as
    // a whole, so the later row's coherent RA 1 model is not mixed into it.
    OwnGalaxyCelestialBody firstRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondRow = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    secondRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    const auto source = createCatalog({firstRow, secondRow, bridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_last over conflicting fixed coordinates from "
        "hd_27."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_last and rejected the incompatible astrometry "
        "of hd_27."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
}

void CatalogIdentityMergeTests::referenceOnlyAstrometryAnchorsTheEarliestRowModel()
{
    // The first two rows carry only a reference position and epoch, with no
    // fixed position and no optional fields, while the bridge carries a full
    // model at RA 5. The earliest row's reference-only model still anchors the
    // survivor, so model selection cannot depend on optional-field presence.
    // The second row agrees and enriches the selected model with its proper
    // motion and parallax.
    OwnGalaxyCelestialBody hipRow =
        makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, std::nullopt, std::nullopt);
    hipRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody hdRow =
        makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt);
    hdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = -10.0,
        .stellarParallaxMas = 5.0,
    };
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
    };
    const auto source = createCatalog({hipRow, hdRow, bridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible fixed coordinates "
        "of bridge_last."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(!survivor->fixedEquatorialValue().has_value());
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 30.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -10.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 5.0);
}

void CatalogIdentityMergeTests::repeatedBridgesKeepTheEarliestRowModel()
{
    // Two bridge rows chain over three donors with conflicting models. The
    // first donor's coherent model wins through both bridges, so neither
    // bridge's own model nor the later donors' models replace it.
    OwnGalaxyCelestialBody hipRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    hipRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hipRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody hdRow = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    hdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hdRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody thirdRow = makeStar("hip_99", "Third", {CatalogIdentifier::make("hip", "99")}, {}, 3.0, 2.0);
    thirdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *thirdRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody firstBridgeRow = makeStar(
        "bridge_one",
        "Bridge one",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    firstBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstBridgeRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondBridgeRow = makeStar(
        "bridge_two",
        "Bridge two",
        {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
        {},
        7.0,
        2.0
    );
    secondBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondBridgeRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    const auto source = createCatalog({hipRow, hdRow, thirdRow, firstBridgeRow, secondBridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "bridge_one."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_one."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_one over conflicting fixed coordinates from "
        "bridge_two."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_one and rejected the incompatible astrometry "
        "of bridge_two."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_two over conflicting fixed coordinates from "
        "hip_99."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_two and rejected the incompatible astrometry "
        "of hip_99."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_two");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(hasRetainedCanonicalId(*survivor, "bridge_one"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_99"));
}

void CatalogIdentityMergeTests::epochCompatibilityDecidesTheOptionalFieldsOfTheEarliestModel()
{
    // The earliest row anchors the model at RA 1 with a declared epoch, and
    // the second row declares the same direction 50 years later through its
    // proper motion, while the bridge supplies RA 5 in both representations.
    // The earliest row's model is selected as a whole; the second row only
    // enriches it when the epoch-converted reference positions still agree.
    constexpr double kEpochGapYears = 50.0;
    constexpr double kProperMotionRaMasPerYear = 1'000.0;
    constexpr double kProperMotionDecMasPerYear = -500.0;
    const double donorRaOffsetHours = kProperMotionRaMasPerYear * kEpochGapYears / 3'600'000.0 / 15.0;
    const double donorDecOffsetDeg = kProperMotionDecMasPerYear * kEpochGapYears / 3'600'000.0;

    const auto makeEarliestRow = [&] {
        OwnGalaxyCelestialBody row = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .referenceEpoch = j2000Epoch(),
            .properMotionRightAscensionMasPerYear = kProperMotionRaMasPerYear,
        };
        return row;
    };
    const auto makeSecondRow = [&](const double properMotionRaMasPerYear) {
        OwnGalaxyCelestialBody row = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial =
                EquatorialCoordinate{
                    .rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = 2.0 + donorDecOffsetDeg
                },
            .referenceEpoch =
                AstronomicalEpoch{
                    .julianDatePart1 = j2000Epoch().julianDatePart1,
                    .julianDatePart2 = kEpochGapYears * 365.25,
                    .timeScale = TimeScale::Tt,
                },
            .properMotionRightAscensionMasPerYear = properMotionRaMasPerYear,
            .properMotionDeclinationMasPerYear = kProperMotionDecMasPerYear,
            .stellarParallaxMas = 5.0,
        };
        return row;
    };
    const auto makeBridgeRow = [&] {
        OwnGalaxyCelestialBody row = makeStar(
            "bridge_last",
            "Last bridge",
            {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
            {},
            5.0,
            2.0
        );
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .referenceEpoch = j2000Epoch(),
        };
        return row;
    };

    const auto compatibleSource =
        createCatalog({makeEarliestRow(), makeSecondRow(kProperMotionRaMasPerYear), makeBridgeRow()}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "bridge_last."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );

    const CatalogCompositionResult compatible = composePrimary(*compatibleSource);

    QVERIFY(compatible.isSuccess());
    QCOMPARE(compatible.bodyCount, std::size_t{1});
    const BaseCelestialBody* compatibleSurvivor = findBodyById(compatible.catalog->bodies(), "bridge_last");
    QVERIFY(compatibleSurvivor != nullptr);
    QVERIFY(compatibleSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& compatibleAstrometry = *compatibleSurvivor->starAstrometryValue();
    QCOMPARE(compatibleAstrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(compatibleAstrometry.referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(compatibleAstrometry.referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(compatibleAstrometry.referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(compatibleAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*compatibleAstrometry.properMotionRightAscensionMasPerYear, kProperMotionRaMasPerYear);
    QVERIFY(compatibleAstrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*compatibleAstrometry.properMotionDeclinationMasPerYear, kProperMotionDecMasPerYear);
    QVERIFY(compatibleAstrometry.stellarParallaxMas.has_value());
    QCOMPARE(*compatibleAstrometry.stellarParallaxMas, 5.0);

    // The same fixture with a proper motion twice the rate the reference
    // offset implies: the epoch conversion leaves a residual, so the second
    // row is rejected and the earliest row's model stays the only description.
    const auto incompatibleSource =
        createCatalog({makeEarliestRow(), makeSecondRow(2.0 * kProperMotionRaMasPerYear), makeBridgeRow()}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "bridge_last."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_last and rejected the incompatible astrometry "
        "of hd_27."
    );

    const CatalogCompositionResult incompatible = composePrimary(*incompatibleSource);

    QVERIFY(incompatible.isSuccess());
    QCOMPARE(incompatible.bodyCount, std::size_t{1});
    const BaseCelestialBody* incompatibleSurvivor = findBodyById(incompatible.catalog->bodies(), "bridge_last");
    QVERIFY(incompatibleSurvivor != nullptr);
    QVERIFY(incompatibleSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& incompatibleAstrometry = *incompatibleSurvivor->starAstrometryValue();
    QCOMPARE(incompatibleAstrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(incompatibleAstrometry.referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(incompatibleAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*incompatibleAstrometry.properMotionRightAscensionMasPerYear, kProperMotionRaMasPerYear);
    QVERIFY(!incompatibleAstrometry.properMotionDeclinationMasPerYear.has_value());
    QVERIFY(!incompatibleAstrometry.stellarParallaxMas.has_value());
}

void CatalogIdentityMergeTests::laterSourceModelReplacesAnEarlierBridgedModel()
{
    // The earlier source deduplicates its two rows into one bridged survivor
    // at RA 1. The later source's own record describes RA 5 in both
    // representations, so it replaces the bridged model as a whole and the
    // incompatible absorbed model is diagnosed.
    OwnGalaxyCelestialBody earlierHip =
        makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    earlierHip.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierHip.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody earlierHd =
        makeStar("a_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    earlierHd.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierHd.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody earlierBridge = makeStar(
        "a_bridge",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        1.0,
        2.0
    );
    earlierBridge.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierBridge.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody laterReplacement =
        makeStar("b_hip_17", "Replacement", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0);
    laterReplacement.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *laterReplacement.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    const auto earlierSource = createCatalog({earlierHip, earlierHd, earlierBridge}, {});
    const auto laterSource = createCatalog({laterReplacement}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_hip_17 over conflicting fixed coordinates from "
        "a_bridge."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of b_hip_17 and rejected the incompatible astrometry of "
        "a_bridge."
    );

    const CatalogCompositionResult result = composeInOrder(*earlierSource, *laterSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_hip_17");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_bridge"));
}

void CatalogIdentityMergeTests::secondPassKeepsTheEarliestRowModelAcrossALateLocalBridge()
{
    // Source A links HIP 17 with HYG 2. Source B's first row describes HYG 2
    // at RA 5; the later local bridge carries the model of its earliest row,
    // which reaches the survivor produced by the first row out of encounter
    // order. The shared merge must replace that survivor's model as a whole
    // with the earlier row's coherent model.
    OwnGalaxyCelestialBody linked = makeStar(
        "a_linked", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0
    );
    linked.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *linked.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody ninthRow =
        makeStar("b_hip_99", "Ninth", {CatalogIdentifier::make("hip", "99")}, {}, 9.0, 2.0);
    ninthRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *ninthRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondRow =
        makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, 5.0, 2.0);
    secondRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody firstRow =
        makeStar("b_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 9.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    OwnGalaxyCelestialBody localBridge = makeStar(
        "b_bridge",
        "Last bridge",
        {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
        {},
        std::nullopt,
        std::nullopt
    );
    const auto sourceA = createCatalog({linked}, {});
    const auto sourceB = createCatalog({ninthRow, secondRow, firstRow, localBridge}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_hyg_2 over conflicting fixed coordinates from "
        "a_linked."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of b_hyg_2 and rejected the incompatible astrometry of "
        "a_linked."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_bridge over conflicting fixed coordinates from "
        "b_hyg_2."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of b_bridge and rejected the incompatible astrometry of "
        "b_hyg_2."
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Ninth"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 9.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 9.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hip", "99"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge"));
}

void CatalogIdentityMergeTests::adoptsAContradictoryDonorsFixedPositionWithoutItsAstrometry()
{
    // The donor row fixes the object at RA 1 while its own astrometry anchors
    // it at RA 5. The bridge row carries no model of its own, so it adopts the
    // donor's model whole; the donor's fixed position survives and its
    // self-contradicting astrometry is rejected instead of accompanying it.
    OwnGalaxyCelestialBody contradictoryDonor =
        makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    contradictoryDonor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 5.0, .declinationDeg = 2.0},
        .referenceEpoch = j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody secondDonor =
        makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt);
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        std::nullopt,
        std::nullopt
    );
    const auto source = createCatalog({contradictoryDonor, secondDonor, bridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_last and rejected the incompatible astrometry of "
        "hip_17."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(!survivor->starAstrometryValue().has_value());
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hd_27"));
}

void CatalogIdentityMergeTests::localBridgeAndLaterReplacementKeepNameMagnitudeAndModel()
{
    // Source A only records that HIP 1 and HYG 2 identify one object. Source B
    // bridges HIP 1 with HIP 3 in its last row, so the row that supplies the
    // object's negative magnitude and its coherent model reaches the survivor
    // behind the row that supplies the name. The later replacing source
    // supplies only a name of its own: the explicit replacement name wins,
    // while the magnitude and both coordinate representations stay with the
    // row that supplied them, and every absorbed or replaced canonical id
    // stays resolvable.
    const auto sourceA = createCatalog(
        {makeStar(
            "a_linked",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );
    const auto sourceB = createCatalog(
        {
            withReferenceModel(makeStar("b_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 4.5, 2.0, -0.5)),
            makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt),
            makeStar("b_hip_3", {}, {CatalogIdentifier::make("hip", "3")}, {}, std::nullopt, std::nullopt),
            makeStar(
                "b_bridge",
                {},
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                std::nullopt,
                std::nullopt
            ),
        },
        {}
    );
    const auto sourceC = createCatalog(
        {makeStar("c_replace", "Replacement", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt)},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult replaced = composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(replaced.catalog->bodies(), "c_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QCOMPARE(survivor->visualMagnitude, -0.5);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 4.5);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 4.5);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hip_1"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hyg_2"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hip_3"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_bridge"));
    QCOMPARE(replaced.sourceIds.front(), std::string("source-2"));
    QCOMPARE(replaced.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));

    // The same shape with the fields spread differently: the name comes from
    // the row that replaces the earlier source, the zero magnitude from the
    // first row through the local bridge, and the coherent model from the
    // third row through the same bridge, while the replacing source supplies
    // no value of its own.
    const auto variedA = createCatalog(
        {makeStar(
            "d_linked",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );
    const auto variedB = createCatalog(
        {
            makeStar("e_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt, 0.0),
            makeStar("e_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt),
            withReferenceModel(makeStar("e_hip_3", {}, {CatalogIdentifier::make("hip", "3")}, {}, 6.5, 2.0)),
            makeStar(
                "e_bridge",
                {},
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                std::nullopt,
                std::nullopt
            ),
        },
        {}
    );
    const auto variedC = createCatalog(
        {makeStar("f_replace", {}, {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt)}, {}
    );
    QVERIFY(variedA != nullptr);
    QVERIFY(variedB != nullptr);
    QVERIFY(variedC != nullptr);

    const CatalogCompositionResult varied = composeAll({variedA.get(), variedB.get(), variedC.get()});

    QVERIFY(varied.isSuccess());
    QCOMPARE(varied.bodyCount, std::size_t{1});
    const BaseCelestialBody* variedSurvivor = findBodyById(varied.catalog->bodies(), "f_replace");
    QVERIFY(variedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(variedSurvivor->displayName), QStringLiteral("Second"));
    QCOMPARE(variedSurvivor->visualMagnitude, 0.0);
    QVERIFY(variedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->rightAscensionHours, 6.5);
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(variedSurvivor->starAstrometryValue().has_value());
    QCOMPARE(variedSurvivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 6.5);
    QCOMPARE(variedSurvivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(hasIdentifier(*variedSurvivor, "hip", "1"));
    QVERIFY(hasIdentifier(*variedSurvivor, "hyg", "2"));
    QVERIFY(hasIdentifier(*variedSurvivor, "hip", "3"));
    QVERIFY(hasRetainedCanonicalId(*variedSurvivor, "d_linked"));
    QVERIFY(hasRetainedCanonicalId(*variedSurvivor, "e_hip_1"));
    QVERIFY(hasRetainedCanonicalId(*variedSurvivor, "e_hyg_2"));
    QVERIFY(hasRetainedCanonicalId(*variedSurvivor, "e_hip_3"));
    QVERIFY(hasRetainedCanonicalId(*variedSurvivor, "e_bridge"));
    QCOMPARE(varied.sourceIds.front(), std::string("source-2"));
    QCOMPARE(varied.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogIdentityMergeTests::samePassDuplicateKeepsMagnitudeInheritedFromAnEarlierSource()
{
    // Source A links HIP 1 and HYG 2 and measures the object at magnitude 7
    // with a coherent model. Source B's first row replaces A's survivor
    // without a magnitude or a model of its own, so it inherits both. Its
    // second row is a later duplicate of the same source that supplies 9 at a
    // conflicting position. A later row of the source that inherited a value
    // never replaces it: the inherited magnitude and model stay authoritative
    // while the replacing row's explicit name wins.
    const auto sourceA = createCatalog(
        {withReferenceModel(makeStar(
            "a_linked",
            "Earlier A",
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            3.0,
            2.0,
            7.0
        ))},
        {}
    );
    const auto sourceB = createCatalog(
        {
            makeStar("b_hyg_2", "Replacement B", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt),
            withReferenceModel(makeStar("b_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 8.0, 2.0, 9.0)),
        },
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of b_hyg_2 over conflicting fixed coordinates from b_hip_1."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of b_hyg_2 and rejected the incompatible astrometry of "
        "b_hip_1."
    );

    const CatalogCompositionResult result = composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement B"));
    QCOMPARE(survivor->visualMagnitude, 7.0);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 3.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 3.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2);
    QVERIFY(hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "b_hip_1"));
    QCOMPARE(result.sourceIds.front(), std::string("source-1"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
}

void CatalogIdentityMergeTests::repeatedBridgesKeepTheEarliestModelUnderALaterReplacement()
{
    // Three rows describe the object at RA 1, 5, and 7, and two bridge rows
    // chain over them with conflicting models of their own. The first row's
    // coherent model wins through both bridges, so neither bridge's model nor
    // the later rows' models replace it. A later source then replaces the
    // bridged survivor with an explicit name and magnitude of its own but no
    // coordinates: the replacement's values win by source precedence while
    // the earliest row's model stays authoritative in both representations.
    const auto sourceA = createCatalog(
        {
            withReferenceModel(makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.0)),
            withReferenceModel(makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 5.0, 2.0, 3.0)),
            withReferenceModel(makeStar("hip_99", "Third", {CatalogIdentifier::make("hip", "99")}, {}, 7.0, 2.0, 4.0)),
            withReferenceModel(makeStar(
                "bridge_one",
                "Bridge one",
                {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
                {},
                9.0,
                2.0,
                5.0
            )),
            withReferenceModel(makeStar(
                "bridge_two",
                "Bridge two",
                {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
                {},
                11.0,
                2.0,
                6.0
            )),
        },
        {}
    );
    const auto sourceB = createCatalog(
        {makeStar(
            "b_replace", "Replacement", {CatalogIdentifier::make("hip", "17")}, {}, std::nullopt, std::nullopt, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 over conflicting fixed coordinates from "
        "bridge_one."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_one."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_one over conflicting fixed coordinates from hd_27."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_one and rejected the incompatible astrometry "
        "of hd_27."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_one over conflicting fixed coordinates from "
        "bridge_two."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_one and rejected the incompatible astrometry "
        "of bridge_two."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_two over conflicting fixed coordinates from hip_99."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of bridge_two and rejected the incompatible astrometry "
        "of hip_99."
    );

    const CatalogCompositionResult result = composeInOrder(*sourceA, *sourceB);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "b_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QCOMPARE(survivor->visualMagnitude, 2.0);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1);
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasIdentifier(*survivor, "hip", "99"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "bridge_two"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "bridge_one"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hd_27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_99"));
    QCOMPARE(result.sourceIds.front(), std::string("second"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"second", "first"}));
}

void CatalogIdentityMergeTests::earliestFixedRowKeepsTheBridgesMeasuredAstrometry()
{
    // The first two rows only fix the shared object at RA 1, while the bridge
    // measures proper motion and parallax at the same direction. The earliest
    // row's fixed-only model replaces the bridge's model as a whole, and the
    // compatible bridge astrometry still enriches the survivor.
    OwnGalaxyCelestialBody hipRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody hdRow = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        1.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
    };
    const auto source = createCatalog({hipRow, hdRow, bridgeRow}, {});
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(!astrometry.referenceEpoch.hasExplicit());
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 5.0);
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hd_27"));
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::duplicateBridgesKeepTheEarliestRowsModelAndMeasurements()
{
    // The bridge is an ordinary duplicate of the fixed-only HIP row, so the
    // earliest row keeps its fixed position while the compatible bridge
    // astrometry enriches it through the per-field merge. Repeating the exact
    // bridge row must leave the survivor unchanged: nothing is lost or
    // restored by the repetition.
    const auto bridgeRow = [] {
        OwnGalaxyCelestialBody row = makeStar(
            "bridge_last",
            "Last bridge",
            {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
            {},
            1.0,
            2.0
        );
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .properMotionRightAscensionMasPerYear = 25.0,
            .stellarParallaxMas = 5.0,
        };
        return row;
    };
    const OwnGalaxyCelestialBody hipRow =
        makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);

    const auto singleBridgeSource = createCatalog({hipRow, bridgeRow()}, {});
    const auto repeatedBridgeSource = createCatalog({hipRow, bridgeRow(), bridgeRow()}, {});
    QVERIFY(singleBridgeSource != nullptr);
    QVERIFY(repeatedBridgeSource != nullptr);

    const CatalogCompositionResult singleBridge = composePrimary(*singleBridgeSource);
    const CatalogCompositionResult repeatedBridge = composePrimary(*repeatedBridgeSource);

    QVERIFY(singleBridge.isSuccess());
    QVERIFY(repeatedBridge.isSuccess());
    QCOMPARE(singleBridge.bodyCount, std::size_t{1});
    QCOMPARE(repeatedBridge.bodyCount, std::size_t{1});
    const BaseCelestialBody* singleSurvivor = findBodyById(singleBridge.catalog->bodies(), "hip_17");
    const BaseCelestialBody* repeatedSurvivor = findBodyById(repeatedBridge.catalog->bodies(), "hip_17");
    QVERIFY(singleSurvivor != nullptr);
    QVERIFY(repeatedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(singleSurvivor->displayName), QStringLiteral("First"));
    QVERIFY(singleSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(singleSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(singleSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(singleSurvivor->starAstrometryValue().has_value());
    QCOMPARE(singleSurvivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(singleSurvivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(singleSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*singleSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(singleSurvivor->starAstrometryValue()->stellarParallaxMas.has_value());
    QCOMPARE(*singleSurvivor->starAstrometryValue()->stellarParallaxMas, 5.0);
    QVERIFY(hasIdentifier(*singleSurvivor, "hip", "17"));
    QVERIFY(hasIdentifier(*singleSurvivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*singleSurvivor, "bridge_last"));
    QCOMPARE(singleBridge.sourceIds.front(), std::string("primary"));
    QCOMPARE(singleBridge.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));

    QCOMPARE(QString::fromStdString(repeatedSurvivor->displayName), QStringLiteral("First"));
    QVERIFY(repeatedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(repeatedSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(repeatedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(repeatedSurvivor->starAstrometryValue().has_value());
    QCOMPARE(repeatedSurvivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(repeatedSurvivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(repeatedSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*repeatedSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(repeatedSurvivor->starAstrometryValue()->stellarParallaxMas.has_value());
    QCOMPARE(*repeatedSurvivor->starAstrometryValue()->stellarParallaxMas, 5.0);
    QCOMPARE(sortedIdentifierKeys(*repeatedSurvivor), sortedIdentifierKeys(*singleSurvivor));
    QCOMPARE(sortedRetainedCanonicalIds(*repeatedSurvivor), sortedRetainedCanonicalIds(*singleSurvivor));
    QCOMPARE(repeatedBridge.sourceIds.front(), std::string("primary"));
    QCOMPARE(repeatedBridge.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::adoptedAstrometrySurvivesWithoutAnIndependentModel()
{
    // The second identifier row describes no coordinates at all, so the
    // bridge still absorbs it and the earliest row's fixed-only model replaces
    // the bridge's model. The compatible bridge astrometry survives with its
    // own reference epoch, declared or not.
    const auto makeHipRow = [] {
        return makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    };
    const auto makeModelLessRow = [] {
        return makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt);
    };
    const auto makeBridgeRow = [](const AstronomicalEpoch referenceEpoch) {
        OwnGalaxyCelestialBody row = makeStar(
            "bridge_last",
            "Last bridge",
            {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
            {},
            1.0,
            2.0
        );
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .referenceEpoch = referenceEpoch,
            .properMotionRightAscensionMasPerYear = 25.0,
            .stellarParallaxMas = 5.0,
        };
        return row;
    };

    const auto undeclaredEpochSource =
        createCatalog({makeHipRow(), makeModelLessRow(), makeBridgeRow(AstronomicalEpoch{})}, {});
    const auto declaredEpochSource = createCatalog({makeHipRow(), makeModelLessRow(), makeBridgeRow(j2000Epoch())}, {});
    QVERIFY(undeclaredEpochSource != nullptr);
    QVERIFY(declaredEpochSource != nullptr);

    const CatalogCompositionResult undeclaredEpoch = composePrimary(*undeclaredEpochSource);
    const CatalogCompositionResult declaredEpoch = composePrimary(*declaredEpochSource);

    QVERIFY(undeclaredEpoch.isSuccess());
    QVERIFY(declaredEpoch.isSuccess());
    QCOMPARE(undeclaredEpoch.bodyCount, std::size_t{1});
    QCOMPARE(declaredEpoch.bodyCount, std::size_t{1});
    const BaseCelestialBody* undeclaredEpochSurvivor = findBodyById(undeclaredEpoch.catalog->bodies(), "bridge_last");
    const BaseCelestialBody* declaredEpochSurvivor = findBodyById(declaredEpoch.catalog->bodies(), "bridge_last");
    QVERIFY(undeclaredEpochSurvivor != nullptr);
    QVERIFY(declaredEpochSurvivor != nullptr);

    QVERIFY(undeclaredEpochSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(undeclaredEpochSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QVERIFY(undeclaredEpochSurvivor->starAstrometryValue().has_value());
    QVERIFY(!undeclaredEpochSurvivor->starAstrometryValue()->referenceEpoch.hasExplicit());
    QVERIFY(undeclaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*undeclaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(undeclaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas.has_value());
    QCOMPARE(*undeclaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas, 5.0);
    QVERIFY(hasIdentifier(*undeclaredEpochSurvivor, "hip", "17"));
    QVERIFY(hasIdentifier(*undeclaredEpochSurvivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*undeclaredEpochSurvivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*undeclaredEpochSurvivor, "hd_27"));
    QCOMPARE(undeclaredEpoch.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));

    QVERIFY(declaredEpochSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(declaredEpochSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QVERIFY(declaredEpochSurvivor->starAstrometryValue().has_value());
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.hasExplicit());
    QCOMPARE(
        declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.julianDatePart1, j2000Epoch().julianDatePart1
    );
    QCOMPARE(
        declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.julianDatePart2, j2000Epoch().julianDatePart2
    );
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*declaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas.has_value());
    QCOMPARE(*declaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas, 5.0);
    QVERIFY(hasIdentifier(*declaredEpochSurvivor, "hip", "17"));
    QVERIFY(hasIdentifier(*declaredEpochSurvivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*declaredEpochSurvivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*declaredEpochSurvivor, "hd_27"));
    QCOMPARE(declaredEpoch.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMergeTests::earliestFixedRowRejectsTheBridgesConflictingAstrometry()
{
    // The bridge describes the object only through astrometry at RA 5, while
    // both identifier rows fix it at RA 1. The earliest row's fixed-only model
    // replaces the bridge's model, and the conflicting astrometry is rejected
    // instead of being combined with the selected fixed position.
    OwnGalaxyCelestialBody hipRow = makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody hdRow = makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        std::nullopt,
        std::nullopt
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 5.0, .declinationDeg = 2.0},
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
    };
    const auto source = createCatalog({hipRow, hdRow, bridgeRow}, {});
    QVERIFY(source != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );

    const CatalogCompositionResult result = composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(!survivor->starAstrometryValue().has_value());
    QVERIFY(hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(hasRetainedCanonicalId(*survivor, "hd_27"));
}

QTEST_APPLESS_MAIN(CatalogIdentityMergeTests)

#include "CatalogIdentityMergeTests.moc"
