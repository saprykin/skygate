#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/IStarCatalog.hpp"

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
using skygate::ephemeris::CatalogComposer;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::DistantCelestialBody;
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
    void fillsMissingMetadataWithoutDiscardingWinnerValues();
    void preservesStableOrdering();
    void mergesLargeFixtureWithoutAllPairsScan();
    void keepsSharedCommonNameDesignationsDistinct();
    void matchesCaseAndPaddingDesignationVariants();
    void keepsSuffixDesignationsDistinct();
    void mergesExplicitCrossIdentifications();
    void primaryDesignationSurvivesBinaryRoundTrip();
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

QTEST_APPLESS_MAIN(CatalogIdentityMergeTests)

#include "CatalogIdentityMergeTests.moc"
