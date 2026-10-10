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
#include <span>
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

class CatalogFieldPrecedenceTests final : public QObject {
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
    void bridgesWithinOneSourceResolveMagnitudeBySupplyingRow();
    void zeroAndNegativeMagnitudesKeepSupplyingRowPrecedence();
    void bridgedDeepSkyMagnitudesFillOnlyMissingValues();
    void replacementMagnitudeKeepsSourcePrecedence();
    void samePassDuplicateKeepsMagnitudeInheritedFromAnEarlierSource();
};

void CatalogFieldPrecedenceTests::fillsMissingMetadataWithoutDiscardingWinnerValues()
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

void CatalogFieldPrecedenceTests::bridgeInheritsMissingMetadataFromTheLatestContributor()
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

void CatalogFieldPrecedenceTests::bridgeWinnerKeepsItsOwnMetadata()
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

void CatalogFieldPrecedenceTests::bridgeInheritsOptionalStarMetadataFromTheLatestContributor()
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

void CatalogFieldPrecedenceTests::reorderingBridgeSourcesChangesInheritedMetadata()
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

void CatalogFieldPrecedenceTests::bridgeAbsorptionUsesSourcePrecedenceAfterReplacements()
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

void CatalogFieldPrecedenceTests::bridgePrefersTheNameOfItsActualSupplyingSource()
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

void CatalogFieldPrecedenceTests::bridgePrefersTheDeepSkyAxisOfItsActualSupplyingSource()
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

void CatalogFieldPrecedenceTests::bridgesWithinOneSourceKeepFirstRowName()
{
    // One source carries three rows: a HIP-only row, an HD-only row, and a
    // bridging row that names both identifiers. The bridge collapses the rows
    // into one object, but the earlier rows keep their value precedence: the
    // first row named the object, so its name survives the bridge.
    const auto source = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}),
            CatalogMergeTestSupport::makeStar("b_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}),
            CatalogMergeTestSupport::makeStar(
                "c_bridge", "Last bridge", {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}
            ),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "a_hip_17"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "b_hd_27"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(bodies, "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hd_27"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogFieldPrecedenceTests::bridgesWithinOneSourceKeepFirstRowDeepSkyValues()
{
    // The NGC row supplies the major axis and an explicit zero position angle,
    // the IC row supplies its own measurements, and the bridging row supplies
    // further values of its own. The first row keeps its fields, the explicit
    // zero stays a real value, and the fields only later rows supply fill the
    // gaps.
    DistantCelestialBody ngcRow = CatalogMergeTestSupport::makeDeepSkyObject(
        "a_ngc_224", {}, {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0
    );
    ngcRow.deepSkyObject->positionAngleDeg = 0.0;
    DistantCelestialBody icRow = CatalogMergeTestSupport::makeDeepSkyObject(
        "b_ic_1", {}, {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0
    );
    icRow.deepSkyObject->positionAngleDeg = 90.0;
    icRow.deepSkyObject->minorAxisArcmin = 5.0;
    DistantCelestialBody bridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "c_bridge", {}, {}, {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 3.0
    );
    bridge.deepSkyObject->positionAngleDeg = 180.0;

    const auto source = CatalogMergeTestSupport::createCatalog({}, {ngcRow, icRow, bridge});
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ic", "1"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 1.0);
    QVERIFY(deepSkyInfo->positionAngleDeg.has_value());
    QCOMPARE(*deepSkyInfo->positionAngleDeg, 0.0);
    QVERIFY(deepSkyInfo->minorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->minorAxisArcmin, 5.0);
}

void CatalogFieldPrecedenceTests::withinSourceBridgeFieldsResolveByEarliestSupplyingRow()
{
    // The first row names the object and supplies the major axis, the second
    // row supplies the position angle and its own values, and the bridge
    // supplies both fields itself. Each field of the survivor must come from
    // its earliest supplying row; reversing the two rows reverses the result.
    DistantCelestialBody ngcRow = CatalogMergeTestSupport::makeDeepSkyObject(
        "a_ngc_224", "Earlier name", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0
    );
    DistantCelestialBody icRow = CatalogMergeTestSupport::makeDeepSkyObject(
        "b_ic_1", "Later name", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0
    );
    icRow.deepSkyObject->positionAngleDeg = 45.0;
    DistantCelestialBody bridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "c_bridge",
        "Bridge name",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        3.0
    );

    const auto forwardSource = CatalogMergeTestSupport::createCatalog({}, {ngcRow, icRow, bridge});
    QVERIFY(forwardSource != nullptr);
    const CatalogCompositionResult forward = CatalogMergeTestSupport::composePrimary(*forwardSource);
    QVERIFY(forward.isSuccess());
    QCOMPARE(forward.bodyCount, std::size_t{1});
    const BaseCelestialBody* forwardSurvivor =
        CatalogMergeTestSupport::findBodyById(forward.catalog->bodies(), "c_bridge");
    QVERIFY(forwardSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(forwardSurvivor->displayName), QStringLiteral("Earlier name"));
    const auto* forwardInfo = forwardSurvivor->deepSkyObjectInfo();
    QVERIFY(forwardInfo != nullptr);
    QVERIFY(forwardInfo->majorAxisArcmin.has_value());
    QCOMPARE(*forwardInfo->majorAxisArcmin, 1.0);
    QVERIFY(forwardInfo->positionAngleDeg.has_value());
    QCOMPARE(*forwardInfo->positionAngleDeg, 45.0);

    const auto reversedSource = CatalogMergeTestSupport::createCatalog({}, {icRow, ngcRow, bridge});
    QVERIFY(reversedSource != nullptr);
    const CatalogCompositionResult reversed = CatalogMergeTestSupport::composePrimary(*reversedSource);
    QVERIFY(reversed.isSuccess());
    QCOMPARE(reversed.bodyCount, std::size_t{1});
    const BaseCelestialBody* reversedSurvivor =
        CatalogMergeTestSupport::findBodyById(reversed.catalog->bodies(), "c_bridge");
    QVERIFY(reversedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(reversedSurvivor->displayName), QStringLiteral("Later name"));
    const auto* reversedInfo = reversedSurvivor->deepSkyObjectInfo();
    QVERIFY(reversedInfo != nullptr);
    QVERIFY(reversedInfo->majorAxisArcmin.has_value());
    QCOMPARE(*reversedInfo->majorAxisArcmin, 2.0);
    QVERIFY(reversedInfo->positionAngleDeg.has_value());
    QCOMPARE(*reversedInfo->positionAngleDeg, 45.0);
}

void CatalogFieldPrecedenceTests::secondWithinSourceBridgeKeepsEarliestRowValues()
{
    // The first bridge absorbs the survivors of rows one and three and is
    // appended at a position later than row two's survivor. The second bridge
    // absorbs that survivor together with row two's own, so accumulator
    // position order would promote row two over row one. Row order decides:
    // the name the first row supplied survives both bridges.
    const auto source = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}),
            CatalogMergeTestSupport::makeStar("b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}),
            CatalogMergeTestSupport::makeStar("c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}),
            CatalogMergeTestSupport::makeStar(
                "d_bridge_13", "Bridge D", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")}
            ),
            CatalogMergeTestSupport::makeStar(
                "e_bridge_all",
                "Last bridge",
                {CatalogIdentifier::make("hyg", "2"), CatalogIdentifier::make("hip", "1")}
            ),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "e_bridge_all");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First A"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hip_1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hyg_2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "c_hip_3"));
}

void CatalogFieldPrecedenceTests::sameSourcePassKeepsEarliestRowNameAcrossALocalBridge()
{
    // Source A links HIP 1 and HYG 2. Source B names the object in its first
    // row, names HYG 2 in its second row, and bridges HIP 1 with HIP 3 in its
    // last row. The bridge absorbs the first row and is visited after the
    // second row replaced A's survivor, so the first row's value arrives out
    // of encounter order; its recorded row origin still wins.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_linked", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")}
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "b_hip_1", "First", {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0
            ),
            CatalogMergeTestSupport::makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}),
            CatalogMergeTestSupport::makeStar("b_hip_3", "Third", {CatalogIdentifier::make("hip", "3")}),
            CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 20.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hip_1"));

    // The same rows without the local bridge already keep the first name, so
    // bridging must not change which value wins.
    const auto controlB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("c_hip_1", "First", {CatalogIdentifier::make("hip", "1")}),
            CatalogMergeTestSupport::makeStar("c_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}),
        },
        {}
    );
    QVERIFY(controlB != nullptr);

    const CatalogCompositionResult control = CatalogMergeTestSupport::composeAll({sourceA.get(), controlB.get()});

    QVERIFY(control.isSuccess());
    QCOMPARE(control.bodyCount, std::size_t{1});
    const BaseCelestialBody* controlSurvivor =
        CatalogMergeTestSupport::findBodyById(control.catalog->bodies(), "c_hip_1");
    QVERIFY(controlSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(controlSurvivor->displayName), QStringLiteral("First"));
}

void CatalogFieldPrecedenceTests::sameSourcePassKeepsEarliestRowDeepSkyValues()
{
    // Source A links NGC 224 with IC 1. Source B measures NGC 224 in its first
    // row and NGC 225 in its third row; its bridge row joins both, so the
    // bridge absorbs the first row's name and major axis. The second row's
    // IC 1 survivor replaces A's object first and the later bridge survivor
    // matches it, so the first row's name and axis still win.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "a_linked_dso",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
            1.0,
            2.0,
            std::nullopt
        )}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {},
        {
            CatalogMergeTestSupport::makeDeepSkyObject(
                "b_ngc_224", "First DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 1.0
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
                "b_ic_1", "Second DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, 2.0
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
                "b_ngc_225", "Third DSO", {}, {CatalogIdentifier::make("ngc", "225")}, 1.0, 2.0, 3.0
            ),
            CatalogMergeTestSupport::makeDeepSkyObject(
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_ic_1");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First DSO"));
    const auto* deepSkyInfo = survivor->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 1.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "ic", "1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked_dso"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge_dso"));
}

void CatalogFieldPrecedenceTests::sameSourcePassMatchResolvesEachFieldByItsSupplyingRow()
{
    // The rows are reversed: HYG 2 names the object in the first row, while
    // HIP 1 supplies its own name and coordinates in the second row, which the
    // bridge absorbs. The name the first row supplied is kept, and the
    // coordinates the winner is missing are filled from the second row, so the
    // same-pass match fills missing fields and each field follows its own
    // supplying row.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_linked",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
            ),
            CatalogMergeTestSupport::makeStar("b_hip_1", "First", {CatalogIdentifier::make("hip", "1")}, {}, 22.0, 2.5),
            CatalogMergeTestSupport::makeStar("b_hip_3", "Third", {CatalogIdentifier::make("hip", "3")}, {}, 23.0, 2.0),
            CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Second"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 22.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.5);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge"));
}

void CatalogFieldPrecedenceTests::secondBridgeSurvivorKeepsRowOriginsUnderLaterReplacement()
{
    // Source A links HIP 1 and HYG 2, and source B bridges its own rows twice
    // inside its dedup pass, the second bridge absorbing the HYG 2 survivor.
    // Source C then replaces the resulting survivor across sources: the
    // replacement's own name wins by source precedence, the coordinates it
    // does not supply stay with the earliest row that supplied them, and the
    // canonical identities of every bridged and replaced row stay resolvable.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_linked", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")}
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "b_hip_1", "First B", {CatalogIdentifier::make("hip", "1")}, {}, 3.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 4.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hip_3", "Third B", {CatalogIdentifier::make("hip", "3")}, {}, 5.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
                "b_bridge_13",
                "Bridge B",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                6.0,
                2.0
            ),
            CatalogMergeTestSupport::makeStar(
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
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_replace", "Replacement", {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 3.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge_13"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge_all"));
}

void CatalogFieldPrecedenceTests::withinSourceBridgeSurvivorKeepsRowOrderAcrossCrossSourceBridge()
{
    // The first source deduplicates three rows into one bridged survivor whose
    // name still comes from its first row. A later source bridges that
    // survivor with its own object, so a higher source rank supplies missing
    // fields while the deduplicated survivor keeps the row order of the source
    // that produced it, and the canonical identities of every absorbed row
    // stay resolvable.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}),
            CatalogMergeTestSupport::makeStar("a_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}),
            CatalogMergeTestSupport::makeStar(
                "a_bridge", "Last bridge", {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}
            ),
        },
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hyg_5", {}, {CatalogIdentifier::make("hyg", "5")})}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hyg", "5")}
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "5"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hd_27"));
    QCOMPARE(result.sourceIds.front(), std::string("source-2"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);

    // The same deduplicated source with a replacement instead of a bridge: the
    // higher-ranked source's own value stays authoritative, and the absorbed
    // survivor keeps all of its identities resolvable.
    const auto replacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("d_hip_17", "Replaced", {CatalogIdentifier::make("hip", "17")})}, {}
    );
    QVERIFY(replacement != nullptr);
    const CatalogCompositionResult replaced = CatalogMergeTestSupport::composeAll({sourceA.get(), replacement.get()});
    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* replacedSurvivor =
        CatalogMergeTestSupport::findBodyById(replaced.catalog->bodies(), "d_hip_17");
    QVERIFY(replacedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(replacedSurvivor->displayName), QStringLiteral("Replaced"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*replacedSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacedSurvivor, "a_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacedSurvivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacedSurvivor, "a_hd_27"));
    QCOMPARE(replaced.sourceIds.front(), std::string("source-1"));
    QCOMPARE(replaced.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
    QVERIFY(replacedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(replacedSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(replacedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
}

void CatalogFieldPrecedenceTests::explicitReplacementValueOutranksAnInheritedValue()
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

void CatalogFieldPrecedenceTests::deepSkyValuesResolveIndependentlyBySupplyingSource()
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

void CatalogFieldPrecedenceTests::absentAndZeroDeepSkyValuesKeepTheirMeaning()
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

void CatalogFieldPrecedenceTests::inheritedAstrometryValuesKeepTheirSupplyingSource()
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

void CatalogFieldPrecedenceTests::inheritedFixedCoordinatesFollowTheirSupplyingSource()
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

void CatalogFieldPrecedenceTests::reorderedReplacementChainChangesTheInheritedName()
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

void CatalogFieldPrecedenceTests::bridgesWithinOneSourceResolveMagnitudeBySupplyingRow()
{
    // The bridging row carries the largest magnitude of the fixture, but the
    // magnitude is enriched metadata like every other field: the earliest row
    // that supplied one keeps it, so the bridge cannot change the composed
    // value. Identity and name keep their own policies on the same rows.
    const auto source = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.0
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "a_hip_17"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "b_hd_27"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(bodies, "c_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QCOMPARE(survivor->visualMagnitude, 1.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hd_27"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogFieldPrecedenceTests::zeroAndNegativeMagnitudesKeepSupplyingRowPrecedence()
{
    // The first row supplies an explicit zero magnitude, the second row a
    // negative one, and both later bridge rows magnitudes of their own. Zero is
    // a value, so the earliest row keeps the magnitude through both local
    // bridges. Giving the two earliest rows the opposite magnitudes moves the
    // winner with them, so the result follows row precedence instead of a
    // preference for one sign.
    const auto zeroFirst = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 0.0
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0, -1.5
            ),
            CatalogMergeTestSupport::makeStar(
                "c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 2.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
                "d_bridge_13",
                "Bridge D",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                1.0,
                2.0,
                -0.5
            ),
            CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult zeroFirstResult = CatalogMergeTestSupport::composePrimary(*zeroFirst);

    QVERIFY(zeroFirstResult.isSuccess());
    QCOMPARE(zeroFirstResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* zeroFirstSurvivor =
        CatalogMergeTestSupport::findBodyById(zeroFirstResult.catalog->bodies(), "e_bridge_all");
    QVERIFY(zeroFirstSurvivor != nullptr);
    QCOMPARE(zeroFirstSurvivor->visualMagnitude, 0.0);
    QCOMPARE(QString::fromStdString(zeroFirstSurvivor->displayName), QStringLiteral("First A"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*zeroFirstSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*zeroFirstSurvivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*zeroFirstSurvivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*zeroFirstSurvivor, "a_hip_1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*zeroFirstSurvivor, "b_hyg_2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*zeroFirstSurvivor, "c_hip_3"));

    const auto negativeFirst = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, -1.5
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Second B", {CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0, 0.0
            ),
            CatalogMergeTestSupport::makeStar(
                "c_hip_3", "Third C", {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 2.0, 2.0
            ),
            CatalogMergeTestSupport::makeStar(
                "d_bridge_13",
                "Bridge D",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hip", "3")},
                {},
                1.0,
                2.0,
                -0.5
            ),
            CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult negativeFirstResult = CatalogMergeTestSupport::composePrimary(*negativeFirst);

    QVERIFY(negativeFirstResult.isSuccess());
    QCOMPARE(negativeFirstResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* negativeFirstSurvivor =
        CatalogMergeTestSupport::findBodyById(negativeFirstResult.catalog->bodies(), "e_bridge_all");
    QVERIFY(negativeFirstSurvivor != nullptr);
    QCOMPARE(negativeFirstSurvivor->visualMagnitude, -1.5);
    QCOMPARE(QString::fromStdString(negativeFirstSurvivor->displayName), QStringLiteral("First A"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*negativeFirstSurvivor, "a_hip_1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*negativeFirstSurvivor, "b_hyg_2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*negativeFirstSurvivor, "c_hip_3"));
}

void CatalogFieldPrecedenceTests::bridgedDeepSkyMagnitudesFillOnlyMissingValues()
{
    // A deep-sky row the parser could not measure carries a NaN magnitude, so
    // it supplies no value and the bridge row's own value stays the starting
    // point. The first row that does supply a magnitude therefore fills the
    // gap, while a later row never overrides an earlier supplying row.
    DistantCelestialBody unmeasuredNgc = CatalogMergeTestSupport::makeDeepSkyObject(
        "a_ngc_224", "First DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    DistantCelestialBody measuredIc = CatalogMergeTestSupport::makeDeepSkyObject(
        "b_ic_1", "Second DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt
    );
    measuredIc.visualMagnitude = 4.5;
    DistantCelestialBody unmeasuredBridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "c_bridge",
        "Bridge DSO",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );

    const auto gapFillingSource =
        CatalogMergeTestSupport::createCatalog({}, {unmeasuredNgc, measuredIc, unmeasuredBridge});
    QVERIFY(gapFillingSource != nullptr);

    const CatalogCompositionResult gapFilling = CatalogMergeTestSupport::composePrimary(*gapFillingSource);

    QVERIFY(gapFilling.isSuccess());
    QCOMPARE(gapFilling.bodyCount, std::size_t{1});
    QCOMPARE(gapFilling.deepSkyObjectCount, std::size_t{1});
    const BaseCelestialBody* gapFillingSurvivor =
        CatalogMergeTestSupport::findBodyById(gapFilling.catalog->bodies(), "c_bridge");
    QVERIFY(gapFillingSurvivor != nullptr);
    QCOMPARE(gapFillingSurvivor->visualMagnitude, 4.5);
    QCOMPARE(QString::fromStdString(gapFillingSurvivor->displayName), QStringLiteral("First DSO"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*gapFillingSurvivor, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*gapFillingSurvivor, "ic", "1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*gapFillingSurvivor, "a_ngc_224"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*gapFillingSurvivor, "b_ic_1"));

    // The earliest row supplies a magnitude of its own and the bridge measures
    // the same object differently, so the bridge must not override it. The
    // middle row supplies no magnitude and stays absent instead of becoming a
    // zero.
    DistantCelestialBody measuredNgc = CatalogMergeTestSupport::makeDeepSkyObject(
        "d_ngc_224", "Measured DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    measuredNgc.visualMagnitude = 2.5;
    DistantCelestialBody unmeasuredIc = CatalogMergeTestSupport::makeDeepSkyObject(
        "e_ic_1", "Unmeasured DSO", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt
    );
    DistantCelestialBody measuredBridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "f_bridge",
        "Bridge DSO",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );
    measuredBridge.visualMagnitude = 7.0;

    const auto measuredSource = CatalogMergeTestSupport::createCatalog({}, {measuredNgc, unmeasuredIc, measuredBridge});
    QVERIFY(measuredSource != nullptr);

    const CatalogCompositionResult measured = CatalogMergeTestSupport::composePrimary(*measuredSource);

    QVERIFY(measured.isSuccess());
    QCOMPARE(measured.bodyCount, std::size_t{1});
    const BaseCelestialBody* measuredSurvivor =
        CatalogMergeTestSupport::findBodyById(measured.catalog->bodies(), "f_bridge");
    QVERIFY(measuredSurvivor != nullptr);
    QCOMPARE(measuredSurvivor->visualMagnitude, 2.5);
    QCOMPARE(QString::fromStdString(measuredSurvivor->displayName), QStringLiteral("Measured DSO"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*measuredSurvivor, "d_ngc_224"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*measuredSurvivor, "e_ic_1"));

    // No row of the fixture supplies a magnitude, so the survivor carries
    // none: an unmeasured body never gains a magnitude from a bridge.
    DistantCelestialBody bareNgc = CatalogMergeTestSupport::makeDeepSkyObject(
        "g_ngc_224", "Bare DSO", {}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, std::nullopt
    );
    DistantCelestialBody bareIc = CatalogMergeTestSupport::makeDeepSkyObject(
        "h_ic_1", "Bare IC", {}, {CatalogIdentifier::make("ic", "1")}, 1.0, 2.0, std::nullopt
    );
    DistantCelestialBody bareBridge = CatalogMergeTestSupport::makeDeepSkyObject(
        "i_bridge",
        "Bare bridge",
        {},
        {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("ic", "1")},
        1.0,
        2.0,
        std::nullopt
    );

    const auto bareSource = CatalogMergeTestSupport::createCatalog({}, {bareNgc, bareIc, bareBridge});
    QVERIFY(bareSource != nullptr);

    const CatalogCompositionResult bare = CatalogMergeTestSupport::composePrimary(*bareSource);

    QVERIFY(bare.isSuccess());
    QCOMPARE(bare.bodyCount, std::size_t{1});
    const BaseCelestialBody* bareSurvivor = CatalogMergeTestSupport::findBodyById(bare.catalog->bodies(), "i_bridge");
    QVERIFY(bareSurvivor != nullptr);
    QVERIFY(std::isnan(bareSurvivor->visualMagnitude));
    QCOMPARE(QString::fromStdString(bareSurvivor->displayName), QStringLiteral("Bare DSO"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*bareSurvivor, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*bareSurvivor, "ic", "1"));
}

void CatalogFieldPrecedenceTests::replacementMagnitudeKeepsSourcePrecedence()
{
    // A replacing source outranks the source it replaces, so its explicit
    // magnitude wins over the replaced survivor's earlier one, while a
    // replacement that supplies no magnitude inherits the replaced value
    // instead of erasing it.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_hip_1", "First A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 1.0
        )},
        {}
    );
    const auto explicitReplacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "b_hip_1", "Replacement B", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0, 2.0
        )},
        {}
    );
    const auto silentReplacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hip_1", "Late C", {CatalogIdentifier::make("hip", "1")})}, {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(explicitReplacement != nullptr);
    QVERIFY(silentReplacement != nullptr);

    const CatalogCompositionResult explicitResult =
        CatalogMergeTestSupport::composeAll({sourceA.get(), explicitReplacement.get()});

    QVERIFY(explicitResult.isSuccess());
    QCOMPARE(explicitResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* explicitSurvivor =
        CatalogMergeTestSupport::findBodyById(explicitResult.catalog->bodies(), "b_hip_1");
    QVERIFY(explicitSurvivor != nullptr);
    QCOMPARE(explicitSurvivor->visualMagnitude, 2.0);
    QCOMPARE(QString::fromStdString(explicitSurvivor->displayName), QStringLiteral("Replacement B"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*explicitSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*explicitSurvivor, "a_hip_1"));

    const CatalogCompositionResult inheritedResult =
        CatalogMergeTestSupport::composeAll({sourceA.get(), silentReplacement.get()});

    QVERIFY(inheritedResult.isSuccess());
    QCOMPARE(inheritedResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* inheritedSurvivor =
        CatalogMergeTestSupport::findBodyById(inheritedResult.catalog->bodies(), "c_hip_1");
    QVERIFY(inheritedSurvivor != nullptr);
    QCOMPARE(inheritedSurvivor->visualMagnitude, 1.0);
    QCOMPARE(QString::fromStdString(inheritedSurvivor->displayName), QStringLiteral("Late C"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*inheritedSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*inheritedSurvivor, "a_hip_1"));

    // The same policies hold through a local bridge chain: the bridged source
    // row that supplied the magnitude first decides its value, and the later
    // replacing source without a magnitude of its own inherits it instead of
    // falling back to the lower-ranked source's earlier value. Name, identity,
    // and magnitude follow their own policies on the same rows.
    const auto firstSource = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_hip_17", "First A", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.5
        )},
        {}
    );
    const auto bridgedSource = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "b_hip_17", "First B", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 2.5
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hd_27", "Second B", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0, 3.5
            ),
            CatalogMergeTestSupport::makeStar(
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
    const auto replacementWithoutMagnitude = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hip_17", "Replacement C", {CatalogIdentifier::make("hip", "17")})}, {}
    );
    QVERIFY(firstSource != nullptr);
    QVERIFY(bridgedSource != nullptr);
    QVERIFY(replacementWithoutMagnitude != nullptr);

    const CatalogCompositionResult chain = CatalogMergeTestSupport::composeAll(
        {firstSource.get(), bridgedSource.get(), replacementWithoutMagnitude.get()}
    );

    QVERIFY(chain.isSuccess());
    QCOMPARE(chain.bodyCount, std::size_t{1});
    const BaseCelestialBody* chainSurvivor = CatalogMergeTestSupport::findBodyById(chain.catalog->bodies(), "c_hip_17");
    QVERIFY(chainSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(chainSurvivor->displayName), QStringLiteral("Replacement C"));
    QCOMPARE(chainSurvivor->visualMagnitude, 2.5);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*chainSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*chainSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*chainSurvivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*chainSurvivor, "b_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*chainSurvivor, "b_hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*chainSurvivor, "b_bridge"));
    QCOMPARE(chain.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogFieldPrecedenceTests::samePassDuplicateKeepsMagnitudeInheritedFromAnEarlierSource()
{
    // Source A links HIP 1 and HYG 2 and measures the object at magnitude 7
    // with a coherent model. Source B's first row replaces A's survivor
    // without a magnitude or a model of its own, so it inherits both. Its
    // second row is a later duplicate of the same source that supplies 9 at a
    // conflicting position. A later row of the source that inherited a value
    // never replaces it: the inherited magnitude and model stay authoritative
    // while the replacing row's explicit name wins.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::withReferenceModel(
            CatalogMergeTestSupport::makeStar(
                "a_linked",
                "Earlier A",
                {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
                {},
                3.0,
                2.0,
                7.0
            )
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Replacement B", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "b_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 8.0, 2.0, 9.0
                )
            ),
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement B"));
    QCOMPARE(survivor->visualMagnitude, 7.0);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 3.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 3.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(
        survivor->starAstrometryValue()->referenceEpoch.julianDatePart1,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart1
    );
    QCOMPARE(
        survivor->starAstrometryValue()->referenceEpoch.julianDatePart2,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart2
    );
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hip_1"));
    QCOMPARE(result.sourceIds.front(), std::string("source-1"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
}

QTEST_APPLESS_MAIN(CatalogFieldPrecedenceTests)

#include "CatalogFieldPrecedenceTests.moc"
