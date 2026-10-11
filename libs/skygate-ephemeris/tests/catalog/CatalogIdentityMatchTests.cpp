#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogComposer;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

namespace {

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

}  // namespace

class CatalogIdentityMatchTests final : public QObject {
    Q_OBJECT

private slots:
    void deduplicatesDuplicateHipStars();
    void deduplicatesDuplicateOpenNgcRows();
    void keepsSameNameUnrelatedObjectsDistinct();
    void keepsAmbiguousAliasMatchesDistinct();
    void mergesAuthoritativeMatchesWithDeepSkyPrecedence();
    void resolvesIdentifierChainsWithinSource();
    void keepsIncompatibleKindsDistinct();
    void preservesStableOrdering();
    void keepsSharedCommonNameDesignationsDistinct();
    void keepsFallbackDesignationsDistinctForCommonName();
    void matchingFallbackRowPreservesWinnerValues();
    void keepsFallbackIncompatibleKindDistinct();
    void augmentCoreKeepsBundledBodyDistinctFromDeepSky();
    void keepsAmbiguousFallbackAliasMatchDistinct();
    void matchesCaseAndPaddingDesignationVariants();
    void keepsSuffixDesignationsDistinct();
    void mergesExplicitCrossIdentifications();
    void primaryDesignationSurvivesBinaryRoundTrip();
    void classifiesUppercaseCanonicalDesignations();
};

void CatalogIdentityMatchTests::deduplicatesDuplicateHipStars()
{
    const auto parsed = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "hip,ra,dec,mag\n"
        "123,1.0,2.0,3.0\n"
        "123,2.0,3.0,4.0\n"
    );
    QVERIFY(parsed.isSuccess());
    QVERIFY(parsed.catalog != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "hip_123"), std::size_t{1});
}

void CatalogIdentityMatchTests::deduplicatesDuplicateOpenNgcRows()
{
    const auto primary = CatalogMergeTestSupport::createCatalog({CatalogMergeTestSupport::makeStar("hip_1")}, {});
    const auto deepSky = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;M;NGC;IC;Identifiers;Common names\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky.isSuccess());
    QVERIFY(deepSky.catalog != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "messier_031"), std::size_t{1});
}

void CatalogIdentityMatchTests::keepsSameNameUnrelatedObjectsDistinct()
{
    const auto primary = CatalogMergeTestSupport::createCatalog({CatalogMergeTestSupport::makeStar("hip_1")}, {});
    const auto deepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject("open_ngc_first", "Andromeda", {}, {}),
            CatalogMergeTestSupport::makeDeepSkyObject("open_ngc_second", "Andromeda", {}, {}),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "open_ngc_first"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "open_ngc_second"), std::size_t{1});
}

void CatalogIdentityMatchTests::keepsAmbiguousAliasMatchesDistinct()
{
    const auto primary = CatalogMergeTestSupport::createCatalog({CatalogMergeTestSupport::makeStar("hip_1")}, {});
    const auto deepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject(
                "ngc_224", "Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
                "ngc_598", "Triangulum", {"M31"}, {CatalogIdentifier::make("ngc", "598")}
            ),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_224"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_598"), std::size_t{1});
}

void CatalogIdentityMatchTests::mergesAuthoritativeMatchesWithDeepSkyPrecedence()
{
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "ngc_224", "Primary Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 3.4
        )}
    );
    const auto deepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
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

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(merged != nullptr);
    QCOMPARE(QString::fromStdString(merged->displayName), QStringLiteral("OpenNGC Andromeda"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "messier", "031"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "M 31"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "NGC 224"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "M31"));
}

void CatalogIdentityMatchTests::resolvesIdentifierChainsWithinSource()
{
    const auto source = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("cat_a_1", {}, {CatalogIdentifier::make("hip", "1")}),
            CatalogMergeTestSupport::makeStar(
                "cat_a_2", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "5")}
            ),
            CatalogMergeTestSupport::makeStar("cat_a_3", {}, {CatalogIdentifier::make("hyg", "5")}),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "cat_a_1"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "cat_a_2"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "cat_a_3"), std::size_t{0});

    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "cat_a_1");
    QVERIFY(merged != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "hyg", "5"));
}

void CatalogIdentityMatchTests::keepsIncompatibleKindsDistinct()
{
    const auto source = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("hip_123", {}, {CatalogIdentifier::make("hip", "123")})},
        {CatalogMergeTestSupport::makeDeepSkyObject("ngc_123", {}, {}, {CatalogIdentifier::make("hip", "123")})}
    );
    QVERIFY(source != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "hip_123"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_123"), std::size_t{1});

    const BaseCelestialBody* star = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "hip_123");
    const BaseCelestialBody* deepSkyObject = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "ngc_123");
    QVERIFY(star != nullptr);
    QVERIFY(deepSkyObject != nullptr);
    QCOMPARE(star->kind, BaseCelestialBody::Kind::Star);
    QCOMPARE(deepSkyObject->kind, BaseCelestialBody::Kind::DeepSkyObject);
}

void CatalogIdentityMatchTests::preservesStableOrdering()
{
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("star_a"),
            CatalogMergeTestSupport::makeStar("star_b"),
        },
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "dso_primary", "Primary DSO", {"M31"}, {CatalogIdentifier::make("messier", "31")}
        )}
    );
    const auto deepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject(
                "dso_new", "New DSO", {}, {CatalogIdentifier::make("ngc", "9999")}
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
                "dso_replacement", "Replacement", {"M 31"}, {CatalogIdentifier::make("messier", "31")}
            ),
        }
    );
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

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

void CatalogIdentityMatchTests::keepsSharedCommonNameDesignationsDistinct()
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
    const auto result = CatalogMergeTestSupport::composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_1"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_2"), std::size_t{1});

    const BaseCelestialBody* first = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "ngc_1");
    const BaseCelestialBody* second = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "ngc_2");
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*first, "ngc", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*second, "ngc", "2"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(first->identity.aliases, "Shared region"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(second->identity.aliases, "Shared region"));
    QVERIFY(first->fixedEquatorialValue().has_value());
    QVERIFY(second->fixedEquatorialValue().has_value());
    QCOMPARE(first->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(second->fixedEquatorialValue()->rightAscensionHours, 10.0);
}

void CatalogIdentityMatchTests::keepsFallbackDesignationsDistinctForCommonName()
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "ngc_1"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "ngc_2"), std::size_t{1});

    const BaseCelestialBody* configured = CatalogMergeTestSupport::findBodyById(bodies, "ngc_1");
    const BaseCelestialBody* appended = CatalogMergeTestSupport::findBodyById(bodies, "ngc_2");
    QVERIFY(configured != nullptr);
    QVERIFY(appended != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*configured, "ngc", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*appended, "ngc", "2"));
    QVERIFY(configured->fixedEquatorialValue().has_value());
    QVERIFY(appended->fixedEquatorialValue().has_value());
    QCOMPARE(configured->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(appended->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.sourceIds.back(), std::string("gap-fill"));
}

void CatalogIdentityMatchTests::matchingFallbackRowPreservesWinnerValues()
{
    // The fallback row carries the same authoritative designation, so the
    // shared identity decision proves both records describe one object.
    // Gap-fill preserves the configured survivor: the fallback's conflicting
    // values, alias, and provenance never replace or corrupt the winner.
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "ngc_224", "Configured Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 100.0
        )}
    );
    const auto fallback = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
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

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(bodies, "ngc_224");
    QVERIFY(winner != nullptr);
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Configured Andromeda"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 2.0);
    const auto* info = winner->deepSkyObjectInfo();
    QVERIFY(info != nullptr);
    QVERIFY(info->majorAxisArcmin.has_value());
    QCOMPARE(*info->majorAxisArcmin, 100.0);
    QVERIFY(!CatalogMergeTestSupport::hasAlias(winner->identity.aliases, "Bundled alias"));
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogIdentityMatchTests::keepsFallbackIncompatibleKindDistinct()
{
    // The fallback deep-sky row resolves through an authoritative identifier
    // shared with a configured star. Incompatible kinds stay distinct with
    // the normal diagnostic instead of silently suppressing the fallback row.
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("hip_123", "Configured star", {CatalogIdentifier::make("hip", "123")})}, {}
    );
    const auto fallback = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "fallback_shared_hip", "Fallback row", {}, {CatalogIdentifier::make("hip", "123")}
        )}
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "hip_123"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "fallback_shared_hip"), std::size_t{1});

    const BaseCelestialBody* star = CatalogMergeTestSupport::findBodyById(bodies, "hip_123");
    const BaseCelestialBody* deepSky = CatalogMergeTestSupport::findBodyById(bodies, "fallback_shared_hip");
    QVERIFY(star != nullptr);
    QVERIFY(deepSky != nullptr);
    QCOMPARE(star->kind, BaseCelestialBody::Kind::Star);
    QCOMPARE(deepSky->kind, BaseCelestialBody::Kind::DeepSkyObject);
}

void CatalogIdentityMatchTests::augmentCoreKeepsBundledBodyDistinctFromDeepSky()
{
    // A configured deep-sky record whose canonical id collides with a bundled
    // core body of another kind must not suppress the bundled augmentation.
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {}, {CatalogMergeTestSupport::makeDeepSkyObject("Mars", "Mars deep sky", {"Mars region"})}
    );
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "Mars"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "mars"), std::size_t{1});

    const BaseCelestialBody* configured = CatalogMergeTestSupport::findBodyById(bodies, "Mars");
    const BaseCelestialBody* bundled = CatalogMergeTestSupport::findBodyById(bodies, "mars");
    QVERIFY(configured != nullptr);
    QVERIFY(bundled != nullptr);
    QCOMPARE(configured->kind, BaseCelestialBody::Kind::DeepSkyObject);
    QCOMPARE(bundled->kind, BaseCelestialBody::Kind::Planet);
}

void CatalogIdentityMatchTests::keepsAmbiguousFallbackAliasMatchDistinct()
{
    // The configured collection already has two different objects sharing one
    // weak alias, so a fallback row resolving only through that alias cannot
    // be proven equivalent to either. The row stays a distinct object with the
    // same diagnostic the normal merge path reports for an ambiguous match.
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject(
                "ngc_224", "Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
                "ngc_598", "Triangulum", {"M31"}, {CatalogIdentifier::make("ngc", "598")}
            ),
        }
    );
    const auto fallback = CatalogMergeTestSupport::createCatalog(
        {}, {CatalogMergeTestSupport::makeDeepSkyObject("fallback_shared_alias", "Fallback row", {"M31"})}
    );
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "ngc_224"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "ngc_598"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "fallback_shared_alias"), std::size_t{1});
}

void CatalogIdentityMatchTests::matchesCaseAndPaddingDesignationVariants()
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

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*padded.catalog, *unpadded.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "ngc_1");
    QVERIFY(merged != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "ngc", "1"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "First region"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "Second region"));
}

void CatalogIdentityMatchTests::keepsSuffixDesignationsDistinct()
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
    const auto result = CatalogMergeTestSupport::composePrimary(*parsed.catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_1"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_1A"), std::size_t{1});

    const BaseCelestialBody* component = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "ngc_1A");
    QVERIFY(component != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*component, "ngc", "1a"));
}

void CatalogIdentityMatchTests::mergesExplicitCrossIdentifications()
{
    const auto openNgc = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA;Dec;M;NGC;IC;Identifiers;Common names\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557;Andromeda Galaxy\n"
    );
    QVERIFY(openNgc.isSuccess());

    const auto referring = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "external_ngc_224", "External NGC 224", {}, {CatalogIdentifier::make("ngc", "224")}, 0.7123, 41.269
        )}
    );
    QVERIFY(referring != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*openNgc.catalog, *referring);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});

    const BaseCelestialBody* merged =
        CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "external_ngc_224");
    QVERIFY(merged != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*merged, "messier", "031"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "Andromeda Galaxy"));
}

void CatalogIdentityMatchTests::primaryDesignationSurvivesBinaryRoundTrip()
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

    const BaseCelestialBody* restoredBody = CatalogMergeTestSupport::findBodyById(restored->bodies(), "ngc_1");
    QVERIFY(restoredBody != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*restoredBody, "ngc", "1"));

    const auto referring = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "external_ngc_1", "External NGC 1", {}, {CatalogIdentifier::make("ngc", "1")}
        )}
    );
    QVERIFY(referring != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*restored, *referring);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "external_ngc_1"), std::size_t{1});
}

void CatalogIdentityMatchTests::classifiesUppercaseCanonicalDesignations()
{
    const auto designation = CatalogIdentifier::designationFromCanonicalId("NGC_224");
    QVERIFY(designation.has_value());
    QCOMPARE(designation->key(), std::string{"ngc:224"});
    QVERIFY(!CatalogIdentifier::designationFromCanonicalId("hip_224").has_value());

    // Records that spell their canonical deep-sky ids in upper case still
    // classify as the designations those ids denote, so a shared descriptive
    // alias keeps them distinct instead of merging them.
    const auto catalog = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject("NGC_224", "Upper case", {"Shared region"}),
            CatalogMergeTestSupport::makeDeepSkyObject("ngc_598", "Lower case", {"Shared region"}),
        }
    );
    QVERIFY(catalog != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_598 distinct from NGC_224 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    const auto result = CatalogMergeTestSupport::composePrimary(*catalog);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, std::size_t{2});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "NGC_224"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "ngc_598"), std::size_t{1});
}

QTEST_APPLESS_MAIN(CatalogIdentityMatchTests)

#include "CatalogIdentityMatchTests.moc"
