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
    void matchesCaseAndPaddingDesignationVariants();
    void keepsSuffixDesignationsDistinct();
    void mergesExplicitCrossIdentifications();
    void primaryDesignationSurvivesBinaryRoundTrip();
    void bridgesIdentifiersAcquiredEarlierInTheSameSourcePass();
    void bridgesAmbiguousAuthoritativeIdentifiersIntoSingleSurvivor();
    void keepsIncompatibleKindsDistinctWhileBridgingSameKindSurvivors();
    void bridgesDeepSkyObjectsWithMetadataUnion();
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
    // R4: the earlier source places the shared HIP 1 object at RA 1 with
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
    // R3: source A establishes that HIP 1 and HD 2 are one object. Source B's
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
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Alpha"));
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-a", "source-c"}));
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
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("A Andromeda"));
    QVERIFY(hasAlias(winner->identity.aliases, "Andromeda Galaxy"));
    QVERIFY(hasAlias(winner->identity.aliases, "Triangulum"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    const auto* deepSkyInfo = winner->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 178.0);
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-a", "source-c"}));
}

QTEST_APPLESS_MAIN(CatalogIdentityMergeTests)

#include "CatalogIdentityMergeTests.moc"
