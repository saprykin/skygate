#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogComposer;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogSourcePrecedenceTests final : public QObject {
    Q_OBJECT

private slots:
    void fillsMissingMetadataWithoutDiscardingWinnerValues();
    void bridgeInheritsMissingMetadataFromTheLatestContributor();
    void bridgeWinnerKeepsItsOwnMetadata();
    void bridgeInheritsOptionalStarMetadataFromTheLatestContributor();
    void reorderingBridgeSourcesChangesInheritedMetadata();
    void bridgeAbsorptionUsesSourcePrecedenceAfterReplacements();
    void bridgePrefersTheNameOfItsActualSupplyingSource();
    void bridgePrefersTheDeepSkyAxisOfItsActualSupplyingSource();
    void explicitReplacementValueOutranksAnInheritedValue();
    void deepSkyValuesResolveIndependentlyBySupplyingSource();
    void absentAndZeroDeepSkyValuesKeepTheirMeaning();
    void inheritedAstrometryValuesKeepTheirSupplyingSource();
    void inheritedFixedCoordinatesFollowTheirSupplyingSource();
    void reorderedReplacementChainChangesTheInheritedName();
};

void CatalogSourcePrecedenceTests::fillsMissingMetadataWithoutDiscardingWinnerValues()
{
    const auto primary = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "ngc_224", "Primary Andromeda", {"M31"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 10.0
        )}
    );
    const auto deepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
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

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(merged != nullptr);
    QCOMPARE(QString::fromStdString(merged->displayName), QStringLiteral("OpenNGC Andromeda"));
    QVERIFY(merged->fixedEquatorialValue().has_value());
    QCOMPARE(merged->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(merged->fixedEquatorialValue()->declinationDeg, 2.0);

    const auto* deepSkyInfo = merged->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 10.0);
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "M31"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(merged->identity.aliases, "M 31"));
}

void CatalogSourcePrecedenceTests::bridgeInheritsMissingMetadataFromTheLatestContributor()
{
    // The bridge resolves the shared object but carries no display name of
    // its own, so its missing name must come from the highest-priority
    // contributor (the later configured source) instead of the earliest
    // accumulator position. Contributors are listed from the winner down to
    // the lowest-priority source, each contributing source once.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Later B"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-b", "source-a"}));
}

void CatalogSourcePrecedenceTests::bridgeWinnerKeepsItsOwnMetadata()
{
    // A value the bridge supplies itself stays authoritative
    // even when every absorbed contributor carries its own value for it.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Bridge C"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-b", "source-a"}));
}

void CatalogSourcePrecedenceTests::bridgeInheritsOptionalStarMetadataFromTheLatestContributor()
{
    // Acceptance with optional domain metadata instead of the display name:
    // the bridge owns no astrometry, so its missing fields come from the
    // highest-priority contributor first and are only then completed by lower
    // priority contributors. The later source supplies the proper motion and
    // parallax, the earlier source only fills the radial velocity gap.
    OwnGalaxyCelestialBody earlier =
        CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .stellarParallaxMas = 1.0,
        .radialVelocityKmPerSecond = -1.0,
    };
    OwnGalaxyCelestialBody later =
        CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0);
    later.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *later.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = -40.0,
        .stellarParallaxMas = 2.0,
    };
    const auto sourceA = CatalogMergeTestSupport::createCatalog({earlier}, {});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({later}, {});
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
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

void CatalogSourcePrecedenceTests::reorderingBridgeSourcesChangesInheritedMetadata()
{
    // The same records with sources A and B swapped. The source configured
    // later is the higher-priority contributor, so the inherited name changes
    // from `Later B` to `Earlier A` and the contributor list swaps accordingly.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Earlier A"));
    QCOMPARE(result.sourceIds.front(), std::string("source-c"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-c", "source-a", "source-b"}));
}

void CatalogSourcePrecedenceTests::bridgeAbsorptionUsesSourcePrecedenceAfterReplacements()
{
    // A replacement runs before the bridge: source-2 replaced source-0's HIP 1
    // survivor, and the replacement's contributor list carries source-0, whose
    // accumulator position is now vacated. The bridge must still inherit from
    // source-2's replacement (the higher-precedence absorbed survivor) and
    // list source-2 before source-1 before source-0. Absorbing in accumulator
    // position order would instead use source-1's name and place source-1
    // before source-2.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto replacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_hip1", "Replacement C", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), replacement.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "a_hip1"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "b_hd2"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "c_hip1"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement C"));
    QCOMPARE(result.sourceIds.front(), std::string("source-3"));
    QCOMPARE(
        result.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogSourcePrecedenceTests::bridgePrefersTheNameOfItsActualSupplyingSource()
{
    // The HIP 1 object is named by the earliest source, and the record that
    // replaces it later carries no name of its own, so the name it holds was
    // supplied by that earliest source. The bridge resolves both objects, and
    // the HD 2 source is the only higher-precedence source that actually
    // supplied a name, so its explicit name wins over the inherited one.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_hip_1", "A inherited", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd_2", "B explicit", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "c_hip_1"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "b_hd_2"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("B explicit"));
    QCOMPARE(
        result.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogSourcePrecedenceTests::bridgePrefersTheDeepSkyAxisOfItsActualSupplyingSource()
{
    // Domain metadata follows the same rule: the NGC object carries a major
    // axis of 1 arcminute, the IC object 2 arcminutes, and the record that
    // replaces the NGC survivor supplies no axis of its own. The bridge
    // resolves both designations, so the axis must come from the IC record,
    // the highest-precedence source that actually supplied one.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0
        )}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0
        )}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
        )}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "d_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ic", "1"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 2.0);
}

void CatalogSourcePrecedenceTests::explicitReplacementValueOutranksAnInheritedValue()
{
    // The record that replaces the NGC survivor supplies its own major axis,
    // so that explicit value stays authoritative even though the IC object
    // also supplies an axis and the bridge supplies none.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0
        )}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0
        )}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 3.0
        )}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "d_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 3.0);
}

void CatalogSourcePrecedenceTests::deepSkyValuesResolveIndependentlyBySupplyingSource()
{
    // Two objects carry different deep-sky values for the same body, and both
    // the record that replaces the NGC survivor and the bridge supply no
    // deep-sky metadata of their own. Every field then resolves on its own:
    // the axis and the position angle follow the higher-precedence source,
    // while the minor axis the IC object alone supplies is kept because no
    // other source offers a value for it. Reordering the two data sources
    // swaps the inherited axis and position angle and leaves the minor axis
    // untouched.
    DistantCelestialBody ngcObject = CatalogMergeTestSupport::makeDeepSkyObject(
        "a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 30.0
    );
    ngcObject.deepSkyObject->positionAngleDeg = 45.0;
    DistantCelestialBody icObject = CatalogMergeTestSupport::makeDeepSkyObject(
        "b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 60.0
    );
    icObject.deepSkyObject->positionAngleDeg = 90.0;
    icObject.deepSkyObject->minorAxisArcmin = 5.0;
    DistantCelestialBody replacement = CatalogMergeTestSupport::makeDeepSkyObject(
        "c_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    replacement.deepSkyObject.reset();
    DistantCelestialBody bridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "d_bridge",
        {},
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    bridge.deepSkyObject.reset();

    const auto sourceNgc = CatalogMergeTestSupport::createCatalog({}, {ngcObject});
    const auto sourceIc = CatalogMergeTestSupport::createCatalog({}, {icObject});
    const auto sourceReplacement = CatalogMergeTestSupport::createCatalog({}, {replacement});
    const auto sourceBridge = CatalogMergeTestSupport::createCatalog({}, {bridge});

    const CatalogCompositionResult ngcFirst = CatalogMergeTestSupport::composeAll(
        {sourceNgc.get(), sourceIc.get(), sourceReplacement.get(), sourceBridge.get()}
    );
    QVERIFY(ngcFirst.isSuccess());
    QCOMPARE(ngcFirst.bodyCount, std::size_t{1});
    const BaseCelestialBody* ngcFirstSurvivor =
        CatalogMergeTestSupport::findBodyById(ngcFirst.catalog->bodies(), "d_bridge");
    QVERIFY(ngcFirstSurvivor != nullptr);
    const auto* deepSkyInfo = ngcFirstSurvivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 60.0);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 90.0);
    QVERIFY(deepSkyInfo->minorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->minorAxisArcmin, 5.0);

    const CatalogCompositionResult icFirst = CatalogMergeTestSupport::composeAll(
        {sourceIc.get(), sourceNgc.get(), sourceReplacement.get(), sourceBridge.get()}
    );
    QVERIFY(icFirst.isSuccess());
    QCOMPARE(icFirst.bodyCount, std::size_t{1});
    const BaseCelestialBody* icFirstSurvivor =
        CatalogMergeTestSupport::findBodyById(icFirst.catalog->bodies(), "d_bridge");
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

void CatalogSourcePrecedenceTests::absentAndZeroDeepSkyValuesKeepTheirMeaning()
{
    // Only the IC object carries a deep-sky measurement, an explicit zero
    // position angle with an unknown kind. The zero is a value, so the bridge
    // keeps it, while the fields nobody supplied stay absent instead of being
    // invented.
    DistantCelestialBody bareNgc = CatalogMergeTestSupport::makeDeepSkyObject(
        "a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    bareNgc.deepSkyObject.reset();
    DistantCelestialBody zeroedIc = CatalogMergeTestSupport::makeDeepSkyObject(
        "b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt
    );
    zeroedIc.deepSkyObject->kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown;
    zeroedIc.deepSkyObject->positionAngleDeg = 0.0;
    DistantCelestialBody bareBridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "c_bridge",
        {},
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    bareBridge.deepSkyObject.reset();

    const auto sourceA = CatalogMergeTestSupport::createCatalog({}, {bareNgc});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({}, {zeroedIc});
    const auto sourceBridge = CatalogMergeTestSupport::createCatalog({}, {bareBridge});

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QCOMPARE(deepSkyInfo->kind, skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 0.0);
    QVERIFY(!deepSkyInfo->majorAxisArcmin.has_value());
    QVERIFY(!deepSkyInfo->minorAxisArcmin.has_value());
}

void CatalogSourcePrecedenceTests::inheritedAstrometryValuesKeepTheirSupplyingSource()
{
    // The earliest source supplies the parallax of the HIP 1 object, and the
    // record that replaces it carries no astrometry of its own, so the model
    // it holds was supplied by that earliest source. The HD 2 object describes
    // the same direction and supplies a proper motion and its own parallax, so
    // both of its values reach the bridge while the surviving reference
    // position stays the one the winner already describes.
    OwnGalaxyCelestialBody earlier =
        CatalogMergeTestSupport::makeStar("a_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .stellarParallaxMas = 5.0,
    };
    OwnGalaxyCelestialBody later =
        CatalogMergeTestSupport::makeStar("b_hd_2", {}, {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0);
    later.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *later.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = 0.0,
        .stellarParallaxMas = 7.0,
    };
    const auto sourceA = CatalogMergeTestSupport::createCatalog({earlier}, {});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({later}, {});
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
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

void CatalogSourcePrecedenceTests::inheritedFixedCoordinatesFollowTheirSupplyingSource()
{
    // Neither the replacement nor the bridge carries a position of its own,
    // and the two absorbed objects disagree about where the shared body is.
    // The position supplied by the higher-precedence source wins, and the
    // merge diagnoses the override.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd_2", {}, {CatalogIdentifier::make("hd", "2")}, {}, 5.0, 6.0)}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt
        )},
        {}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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
    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get(), bridge.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "d_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 6.0);
}

void CatalogSourcePrecedenceTests::reorderedReplacementChainChangesTheInheritedName()
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
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_hip_1", "A inherited", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd_2", "B explicit", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)},
        {}
    );
    const auto replacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto bridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "d_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult explicitSourceHigher =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), replacement.get(), bridge.get()});

    QVERIFY(explicitSourceHigher.isSuccess());
    QCOMPARE(explicitSourceHigher.bodyCount, std::size_t{1});
    const BaseCelestialBody* explicitSurvivor =
        CatalogMergeTestSupport::findBodyById(explicitSourceHigher.catalog->bodies(), "d_bridge");
    QVERIFY(explicitSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*explicitSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*explicitSurvivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(explicitSurvivor->displayName), QStringLiteral("B explicit"));

    const CatalogCompositionResult inheritedSourceHigher =
        CatalogMergeTestSupport::composeAll({sourceB.get(), sourceA.get(), replacement.get(), bridge.get()});

    QVERIFY(inheritedSourceHigher.isSuccess());
    QCOMPARE(inheritedSourceHigher.bodyCount, std::size_t{1});
    const BaseCelestialBody* inheritedSurvivor =
        CatalogMergeTestSupport::findBodyById(inheritedSourceHigher.catalog->bodies(), "d_bridge");
    QVERIFY(inheritedSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*inheritedSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*inheritedSurvivor, "hd", "2"));
    QCOMPARE(QString::fromStdString(inheritedSurvivor->displayName), QStringLiteral("A inherited"));
}

QTEST_APPLESS_MAIN(CatalogSourcePrecedenceTests)

#include "CatalogSourcePrecedenceTests.moc"
