#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogRowOriginTests final : public QObject {
    Q_OBJECT

private slots:
    void bridgesWithinOneSourceKeepFirstRowName();
    void bridgesWithinOneSourceKeepFirstRowDeepSkyValues();
    void withinSourceBridgeFieldsResolveByEarliestRow();
    void secondWithinSourceBridgeKeepsEarliestRowValues();
    void sameSourcePassKeepsEarliestRowNameAcrossBridge();
    void sameSourcePassKeepsEarliestRowDeepSkyValues();
    void sameSourcePassMatchResolvesFieldsBySupplyingRow();
    void secondBridgeSurvivorKeepsOriginsUnderReplacement();
    void withinSourceSurvivorKeepsRowOrderAcrossBridges();
};

void CatalogRowOriginTests::bridgesWithinOneSourceKeepFirstRowName()
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

void CatalogRowOriginTests::bridgesWithinOneSourceKeepFirstRowDeepSkyValues()
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

void CatalogRowOriginTests::withinSourceBridgeFieldsResolveByEarliestRow()
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

void CatalogRowOriginTests::secondWithinSourceBridgeKeepsEarliestRowValues()
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

void CatalogRowOriginTests::sameSourcePassKeepsEarliestRowNameAcrossBridge()
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

void CatalogRowOriginTests::sameSourcePassKeepsEarliestRowDeepSkyValues()
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

void CatalogRowOriginTests::sameSourcePassMatchResolvesFieldsBySupplyingRow()
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

void CatalogRowOriginTests::secondBridgeSurvivorKeepsOriginsUnderReplacement()
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

void CatalogRowOriginTests::withinSourceSurvivorKeepsRowOrderAcrossBridges()
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

QTEST_APPLESS_MAIN(CatalogRowOriginTests)

#include "CatalogRowOriginTests.moc"
