#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

using skygate::core::AstronomicalEpoch;
using skygate::core::EquatorialCoordinate;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::OwnGalaxyCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogModelReplacementTests final : public QObject {
    Q_OBJECT

private slots:
    void repeatedBridgesKeepTheEarliestRowModel();
    void laterSourceModelReplacesAnEarlierBridgedModel();
    void secondPassKeepsTheEarliestRowModelAcrossALateLocalBridge();
    void localBridgeAndLaterReplacementKeepNameMagnitudeAndModel();
    void repeatedBridgesKeepTheEarliestModelUnderALaterReplacement();
    void earliestFixedRowKeepsTheBridgesMeasuredAstrometry();
    void duplicateBridgesKeepTheEarliestRowsModelAndMeasurements();
    void adoptedAstrometrySurvivesWithoutAnIndependentModel();
    void earliestFixedRowRejectsTheBridgesConflictingAstrometry();
    void repairedBridgeKeepsItsAstrometryUnderALaterReplacement();
    void repairedBridgeAstrometryKeepsItsSupplyingRowsAcrossALaterBridge();
};

void CatalogModelReplacementTests::repeatedBridgesKeepTheEarliestRowModel()
{
    // Two bridge rows chain over three donors with conflicting models. The
    // first donor's coherent model wins through both bridges, so neither
    // bridge's own model nor the later donors' models replace it.
    OwnGalaxyCelestialBody hipRow =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    hipRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hipRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody hdRow =
        CatalogMergeTestSupport::makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    hdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *hdRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody thirdRow =
        CatalogMergeTestSupport::makeStar("hip_99", "Third", {CatalogIdentifier::make("hip", "99")}, {}, 3.0, 2.0);
    thirdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *thirdRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody firstBridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_one",
        "Bridge one",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    firstBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstBridgeRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondBridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_two",
        "Bridge two",
        {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
        {},
        7.0,
        2.0
    );
    secondBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondBridgeRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    const auto source =
        CatalogMergeTestSupport::createCatalog({hipRow, hdRow, thirdRow, firstBridgeRow, secondBridgeRow}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "bridge_two");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "bridge_one"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_99"));
}

void CatalogModelReplacementTests::laterSourceModelReplacesAnEarlierBridgedModel()
{
    // The earlier source deduplicates its two rows into one bridged survivor
    // at RA 1. The later source's own record describes RA 5 in both
    // representations, so it replaces the bridged model as a whole and the
    // incompatible absorbed model is diagnosed.
    OwnGalaxyCelestialBody earlierHip =
        CatalogMergeTestSupport::makeStar("a_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    earlierHip.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierHip.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody earlierHd =
        CatalogMergeTestSupport::makeStar("a_hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    earlierHd.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierHd.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody earlierBridge = CatalogMergeTestSupport::makeStar(
        "a_bridge",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        1.0,
        2.0
    );
    earlierBridge.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlierBridge.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody laterReplacement = CatalogMergeTestSupport::makeStar(
        "b_hip_17", "Replacement", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0
    );
    laterReplacement.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *laterReplacement.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    const auto earlierSource = CatalogMergeTestSupport::createCatalog({earlierHip, earlierHd, earlierBridge}, {});
    const auto laterSource = CatalogMergeTestSupport::createCatalog({laterReplacement}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*earlierSource, *laterSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_hip_17");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_bridge"));
}

void CatalogModelReplacementTests::secondPassKeepsTheEarliestRowModelAcrossALateLocalBridge()
{
    // Source A links HIP 17 with HYG 2. Source B's first row describes HYG 2
    // at RA 5; the later local bridge carries the model of its earliest row,
    // which reaches the survivor produced by the first row out of encounter
    // order. The shared merge must replace that survivor's model as a whole
    // with the earlier row's coherent model.
    OwnGalaxyCelestialBody linked = CatalogMergeTestSupport::makeStar(
        "a_linked", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hyg", "2")}, {}, 1.0, 2.0
    );
    linked.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *linked.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody ninthRow =
        CatalogMergeTestSupport::makeStar("b_hip_99", "Ninth", {CatalogIdentifier::make("hip", "99")}, {}, 9.0, 2.0);
    ninthRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *ninthRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondRow =
        CatalogMergeTestSupport::makeStar("b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, 5.0, 2.0);
    secondRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody firstRow =
        CatalogMergeTestSupport::makeStar("b_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 9.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody localBridge = CatalogMergeTestSupport::makeStar(
        "b_bridge",
        "Last bridge",
        {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
        {},
        std::nullopt,
        std::nullopt
    );
    const auto sourceA = CatalogMergeTestSupport::createCatalog({linked}, {});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({ninthRow, secondRow, firstRow, localBridge}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_hyg_2");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Ninth"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 9.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 9.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge"));
}

void CatalogModelReplacementTests::localBridgeAndLaterReplacementKeepNameMagnitudeAndModel()
{
    // Source A only records that HIP 1 and HYG 2 identify one object. Source B
    // bridges HIP 1 with HIP 3 in its last row, so the row that supplies the
    // object's negative magnitude and its coherent model reaches the survivor
    // behind the row that supplies the name. The later replacing source
    // supplies only a name of its own: the explicit replacement name wins,
    // while the magnitude and both coordinate representations stay with the
    // row that supplied them, and every absorbed or replaced canonical id
    // stays resolvable.
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
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "b_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 4.5, 2.0, -0.5
                )
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
            ),
            CatalogMergeTestSupport::makeStar(
                "b_hip_3", {}, {CatalogIdentifier::make("hip", "3")}, {}, std::nullopt, std::nullopt
            ),
            CatalogMergeTestSupport::makeStar(
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
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_replace", "Replacement", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);
    QVERIFY(sourceC != nullptr);

    const CatalogCompositionResult replaced =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(replaced.catalog->bodies(), "c_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QCOMPARE(survivor->visualMagnitude, -0.5);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 4.5);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 4.5);
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
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hip_1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hyg_2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_hip_3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "b_bridge"));
    QCOMPARE(replaced.sourceIds.front(), std::string("source-2"));
    QCOMPARE(replaced.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));

    // The same shape with the fields spread differently: the name comes from
    // the row that replaces the earlier source, the zero magnitude from the
    // first row through the local bridge, and the coherent model from the
    // third row through the same bridge, while the replacing source supplies
    // no value of its own.
    const auto variedA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "d_linked",
            {},
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "2")},
            {},
            std::nullopt,
            std::nullopt
        )},
        {}
    );
    const auto variedB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar(
                "e_hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, std::nullopt, std::nullopt, 0.0
            ),
            CatalogMergeTestSupport::makeStar(
                "e_hyg_2", "Second", {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar("e_hip_3", {}, {CatalogIdentifier::make("hip", "3")}, {}, 6.5, 2.0)
            ),
            CatalogMergeTestSupport::makeStar(
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
    const auto variedC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "f_replace", {}, {CatalogIdentifier::make("hyg", "2")}, {}, std::nullopt, std::nullopt
        )},
        {}
    );
    QVERIFY(variedA != nullptr);
    QVERIFY(variedB != nullptr);
    QVERIFY(variedC != nullptr);

    const CatalogCompositionResult varied =
        CatalogMergeTestSupport::composeAll({variedA.get(), variedB.get(), variedC.get()});

    QVERIFY(varied.isSuccess());
    QCOMPARE(varied.bodyCount, std::size_t{1});
    const BaseCelestialBody* variedSurvivor =
        CatalogMergeTestSupport::findBodyById(varied.catalog->bodies(), "f_replace");
    QVERIFY(variedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(variedSurvivor->displayName), QStringLiteral("Second"));
    QCOMPARE(variedSurvivor->visualMagnitude, 0.0);
    QVERIFY(variedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->rightAscensionHours, 6.5);
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(variedSurvivor->starAstrometryValue().has_value());
    QCOMPARE(variedSurvivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 6.5);
    QCOMPARE(variedSurvivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hyg", "2"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hip", "3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "d_linked"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "e_hip_1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "e_hyg_2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "e_hip_3"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "e_bridge"));
    QCOMPARE(varied.sourceIds.front(), std::string("source-2"));
    QCOMPARE(varied.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogModelReplacementTests::repeatedBridgesKeepTheEarliestModelUnderALaterReplacement()
{
    // Three rows describe the object at RA 1, 5, and 7, and two bridge rows
    // chain over them with conflicting models of their own. The first row's
    // coherent model wins through both bridges, so neither bridge's model nor
    // the later rows' models replace it. A later source then replaces the
    // bridged survivor with an explicit name and magnitude of its own but no
    // coordinates: the replacement's values win by source precedence while
    // the earliest row's model stays authoritative in both representations.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 1.0
                )
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 5.0, 2.0, 3.0
                )
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "hip_99", "Third", {CatalogIdentifier::make("hip", "99")}, {}, 7.0, 2.0, 4.0
                )
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "bridge_one",
                    "Bridge one",
                    {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
                    {},
                    9.0,
                    2.0,
                    5.0
                )
            ),
            CatalogMergeTestSupport::withReferenceModel(
                CatalogMergeTestSupport::makeStar(
                    "bridge_two",
                    "Bridge two",
                    {CatalogIdentifier::make("hip", "99"), CatalogIdentifier::make("hip", "17")},
                    {},
                    11.0,
                    2.0,
                    6.0
                )
            ),
        },
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*sourceA, *sourceB);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QCOMPARE(survivor->visualMagnitude, 2.0);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(
        survivor->starAstrometryValue()->referenceEpoch.julianDatePart1,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart1
    );
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "bridge_two"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "bridge_one"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_99"));
    QCOMPARE(result.sourceIds.front(), std::string("second"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"second", "first"}));
}

void CatalogModelReplacementTests::earliestFixedRowKeepsTheBridgesMeasuredAstrometry()
{
    // The first two rows only fix the shared object at RA 1, while the bridge
    // measures proper motion and parallax at the same direction. The earliest
    // row's fixed-only model replaces the bridge's model as a whole, and the
    // compatible bridge astrometry still enriches the survivor.
    OwnGalaxyCelestialBody hipRow =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody hdRow =
        CatalogMergeTestSupport::makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
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
    const auto source = CatalogMergeTestSupport::createCatalog({hipRow, hdRow, bridgeRow}, {});
    QVERIFY(source != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "bridge_last");
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
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hd_27"));
    QCOMPARE(result.sourceIds.front(), std::string("primary"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogModelReplacementTests::duplicateBridgesKeepTheEarliestRowsModelAndMeasurements()
{
    // The bridge is an ordinary duplicate of the fixed-only HIP row, so the
    // earliest row keeps its fixed position while the compatible bridge
    // astrometry enriches it through the per-field merge. Repeating the exact
    // bridge row must leave the survivor unchanged: nothing is lost or
    // restored by the repetition.
    const auto bridgeRow = [] {
        OwnGalaxyCelestialBody row = CatalogMergeTestSupport::makeStar(
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
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);

    const auto singleBridgeSource = CatalogMergeTestSupport::createCatalog({hipRow, bridgeRow()}, {});
    const auto repeatedBridgeSource = CatalogMergeTestSupport::createCatalog({hipRow, bridgeRow(), bridgeRow()}, {});
    QVERIFY(singleBridgeSource != nullptr);
    QVERIFY(repeatedBridgeSource != nullptr);

    const CatalogCompositionResult singleBridge = CatalogMergeTestSupport::composePrimary(*singleBridgeSource);
    const CatalogCompositionResult repeatedBridge = CatalogMergeTestSupport::composePrimary(*repeatedBridgeSource);

    QVERIFY(singleBridge.isSuccess());
    QVERIFY(repeatedBridge.isSuccess());
    QCOMPARE(singleBridge.bodyCount, std::size_t{1});
    QCOMPARE(repeatedBridge.bodyCount, std::size_t{1});
    const BaseCelestialBody* singleSurvivor =
        CatalogMergeTestSupport::findBodyById(singleBridge.catalog->bodies(), "hip_17");
    const BaseCelestialBody* repeatedSurvivor =
        CatalogMergeTestSupport::findBodyById(repeatedBridge.catalog->bodies(), "hip_17");
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
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*singleSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*singleSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*singleSurvivor, "bridge_last"));
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
    QCOMPARE(
        CatalogMergeTestSupport::sortedIdentifierKeys(*repeatedSurvivor),
        CatalogMergeTestSupport::sortedIdentifierKeys(*singleSurvivor)
    );
    QCOMPARE(
        CatalogMergeTestSupport::sortedRetainedCanonicalIds(*repeatedSurvivor),
        CatalogMergeTestSupport::sortedRetainedCanonicalIds(*singleSurvivor)
    );
    QCOMPARE(repeatedBridge.sourceIds.front(), std::string("primary"));
    QCOMPARE(repeatedBridge.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogModelReplacementTests::adoptedAstrometrySurvivesWithoutAnIndependentModel()
{
    // The second identifier row describes no coordinates at all, so the
    // bridge still absorbs it and the earliest row's fixed-only model replaces
    // the bridge's model. The compatible bridge astrometry survives with its
    // own reference epoch, declared or not.
    const auto makeHipRow = [] {
        return CatalogMergeTestSupport::makeStar(
            "hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0
        );
    };
    const auto makeModelLessRow = [] {
        return CatalogMergeTestSupport::makeStar(
            "hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt
        );
    };
    const auto makeBridgeRow = [](const AstronomicalEpoch referenceEpoch) {
        OwnGalaxyCelestialBody row = CatalogMergeTestSupport::makeStar(
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

    const auto undeclaredEpochSource = CatalogMergeTestSupport::createCatalog(
        {makeHipRow(), makeModelLessRow(), makeBridgeRow(AstronomicalEpoch{})}, {}
    );
    const auto declaredEpochSource = CatalogMergeTestSupport::createCatalog(
        {makeHipRow(), makeModelLessRow(), makeBridgeRow(CatalogMergeTestSupport::j2000Epoch())}, {}
    );
    QVERIFY(undeclaredEpochSource != nullptr);
    QVERIFY(declaredEpochSource != nullptr);

    const CatalogCompositionResult undeclaredEpoch = CatalogMergeTestSupport::composePrimary(*undeclaredEpochSource);
    const CatalogCompositionResult declaredEpoch = CatalogMergeTestSupport::composePrimary(*declaredEpochSource);

    QVERIFY(undeclaredEpoch.isSuccess());
    QVERIFY(declaredEpoch.isSuccess());
    QCOMPARE(undeclaredEpoch.bodyCount, std::size_t{1});
    QCOMPARE(declaredEpoch.bodyCount, std::size_t{1});
    const BaseCelestialBody* undeclaredEpochSurvivor =
        CatalogMergeTestSupport::findBodyById(undeclaredEpoch.catalog->bodies(), "bridge_last");
    const BaseCelestialBody* declaredEpochSurvivor =
        CatalogMergeTestSupport::findBodyById(declaredEpoch.catalog->bodies(), "bridge_last");
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
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*undeclaredEpochSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*undeclaredEpochSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*undeclaredEpochSurvivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*undeclaredEpochSurvivor, "hd_27"));
    QCOMPARE(undeclaredEpoch.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));

    QVERIFY(declaredEpochSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(declaredEpochSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QVERIFY(declaredEpochSurvivor->starAstrometryValue().has_value());
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.hasExplicit());
    QCOMPARE(
        declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.julianDatePart1,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart1
    );
    QCOMPARE(
        declaredEpochSurvivor->starAstrometryValue()->referenceEpoch.julianDatePart2,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart2
    );
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*declaredEpochSurvivor->starAstrometryValue()->properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(declaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas.has_value());
    QCOMPARE(*declaredEpochSurvivor->starAstrometryValue()->stellarParallaxMas, 5.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*declaredEpochSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*declaredEpochSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*declaredEpochSurvivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*declaredEpochSurvivor, "hd_27"));
    QCOMPARE(declaredEpoch.contributorSourceIds.front(), (std::vector<std::string>{"primary"}));
}

void CatalogModelReplacementTests::earliestFixedRowRejectsTheBridgesConflictingAstrometry()
{
    // The bridge describes the object only through astrometry at RA 5, while
    // both identifier rows fix it at RA 1. The earliest row's fixed-only model
    // replaces the bridge's model, and the conflicting astrometry is rejected
    // instead of being combined with the selected fixed position.
    OwnGalaxyCelestialBody hipRow =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody hdRow =
        CatalogMergeTestSupport::makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
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
    const auto source = CatalogMergeTestSupport::createCatalog({hipRow, hdRow, bridgeRow}, {});
    QVERIFY(source != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of hip_17 and rejected the incompatible astrometry of "
        "bridge_last."
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(!survivor->starAstrometryValue().has_value());
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "hd_27"));
}

void CatalogModelReplacementTests::repairedBridgeKeepsItsAstrometryUnderALaterReplacement()
{
    // The earliest row fixes the object at RA 1 and carries its name and
    // magnitude, while the bridge row measures proper motion, parallax, and
    // radial velocity at the same direction. Selecting the earliest row's
    // fixed-only model keeps the bridge's compatible measurements. The later
    // replacing source then supplies a name, a magnitude, and one measured
    // field of its own: the explicit replacement values win by source
    // precedence, while every measured field the replacement leaves open stays
    // with the earlier row that supplied it.
    OwnGalaxyCelestialBody donor = CatalogMergeTestSupport::makeStar(
        "a_hip_17", "Donor", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 3.0
    );
    OwnGalaxyCelestialBody secondIdentifier =
        CatalogMergeTestSupport::makeStar("a_hd_27", {}, {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "a_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
        .radialVelocityKmPerSecond = -5.5,
    };
    OwnGalaxyCelestialBody replacement = CatalogMergeTestSupport::makeStar(
        "b_replace", "Replacement", {CatalogIdentifier::make("hip", "17")}, {}, std::nullopt, std::nullopt, 2.0
    );
    replacement.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .stellarParallaxMas = 9.0,
    };
    const auto sourceA = CatalogMergeTestSupport::createCatalog({donor, secondIdentifier, bridgeRow}, {});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({replacement}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "b_replace");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Replacement"));
    QCOMPARE(survivor->visualMagnitude, 2.0);
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 2.0);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 9.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, -5.5);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "a_bridge"));
    QCOMPARE(result.sourceIds.front(), std::string("source-1"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));

    // The same values with the optional fields assigned to different rows: the
    // bridge row carries the name, the earliest row carries the magnitude, and
    // the later replacement supplies proper motion instead of parallax. The
    // replacement still wins where it supplies a value, and the measured
    // fields it leaves open stay with the bridge row.
    OwnGalaxyCelestialBody variedDonor =
        CatalogMergeTestSupport::makeStar("c_hip_17", {}, {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0, 3.0);
    OwnGalaxyCelestialBody variedSecondIdentifier =
        CatalogMergeTestSupport::makeStar("c_hd_27", {}, {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody variedBridgeRow = CatalogMergeTestSupport::makeStar(
        "c_bridge", "Bridge", {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0
    );
    variedBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *variedBridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
        .radialVelocityKmPerSecond = -5.5,
    };
    OwnGalaxyCelestialBody variedReplacement = CatalogMergeTestSupport::makeStar(
        "d_replace", {}, {CatalogIdentifier::make("hip", "17")}, {}, std::nullopt, std::nullopt, 2.0
    );
    variedReplacement.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .properMotionRightAscensionMasPerYear = 9.0,
    };
    const auto variedSourceA =
        CatalogMergeTestSupport::createCatalog({variedDonor, variedSecondIdentifier, variedBridgeRow}, {});
    const auto variedSourceB = CatalogMergeTestSupport::createCatalog({variedReplacement}, {});
    QVERIFY(variedSourceA != nullptr);
    QVERIFY(variedSourceB != nullptr);

    const CatalogCompositionResult varied =
        CatalogMergeTestSupport::composeAll({variedSourceA.get(), variedSourceB.get()});

    QVERIFY(varied.isSuccess());
    QCOMPARE(varied.bodyCount, std::size_t{1});
    const BaseCelestialBody* variedSurvivor =
        CatalogMergeTestSupport::findBodyById(varied.catalog->bodies(), "d_replace");
    QVERIFY(variedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(variedSurvivor->displayName), QStringLiteral("Bridge"));
    QCOMPARE(variedSurvivor->visualMagnitude, 2.0);
    QVERIFY(variedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(variedSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& variedAstrometry = *variedSurvivor->starAstrometryValue();
    QVERIFY(variedAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*variedAstrometry.properMotionRightAscensionMasPerYear, 9.0);
    QVERIFY(variedAstrometry.stellarParallaxMas.has_value());
    QCOMPARE(*variedAstrometry.stellarParallaxMas, 5.0);
    QVERIFY(variedAstrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*variedAstrometry.radialVelocityKmPerSecond, -5.5);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hd", "27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "c_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "c_hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "c_bridge"));
    QCOMPARE(varied.sourceIds.front(), std::string("source-1"));
    QCOMPARE(varied.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
}

void CatalogModelReplacementTests::repairedBridgeAstrometryKeepsItsSupplyingRowsAcrossALaterBridge()
{
    // The repaired bridge keeps each measured field with the row that supplied
    // it, so a later bridge row that absorbs it together with another measured
    // row of the same source resolves every field by its true origin: the
    // earlier supplying row wins, while the later bridge's own explicit field
    // still outranks both by source precedence.
    OwnGalaxyCelestialBody fixedRow =
        CatalogMergeTestSupport::makeStar("e_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody measuredRow = CatalogMergeTestSupport::makeStar(
        "e_hip_99", {}, {CatalogIdentifier::make("hip", "99")}, {}, std::nullopt, std::nullopt
    );
    measuredRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .properMotionRightAscensionMasPerYear = 10.0,
        .stellarParallaxMas = 2.0,
        .radialVelocityKmPerSecond = 12.0,
    };
    OwnGalaxyCelestialBody secondFixedRow =
        CatalogMergeTestSupport::makeStar("e_hd_27", {}, {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "e_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
    };
    OwnGalaxyCelestialBody laterBridgeRow = CatalogMergeTestSupport::makeStar(
        "f_later_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
    );
    laterBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *laterBridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 9.0,
    };
    const auto sourceA = CatalogMergeTestSupport::createCatalog({fixedRow, measuredRow, secondFixedRow, bridgeRow}, {});
    const auto sourceB = CatalogMergeTestSupport::createCatalog({laterBridgeRow}, {});
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get()});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    const BaseCelestialBody* survivor =
        CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "f_later_bridge");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(survivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 9.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 2.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, 12.0);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "e_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "e_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "e_hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "e_hip_99"));
    QCOMPARE(result.sourceIds.front(), std::string("source-1"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));

    // The same values with the row order reversed and the later bridge
    // supplying parallax instead of proper motion: the repaired bridge's row
    // is now the earlier one, so its measured fields win against the other
    // measured row, and the explicit later parallax still replaces the
    // inherited one.
    OwnGalaxyCelestialBody variedFixedRow =
        CatalogMergeTestSupport::makeStar("h_hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody variedSecondFixedRow =
        CatalogMergeTestSupport::makeStar("h_hd_27", {}, {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    OwnGalaxyCelestialBody variedBridgeRow = CatalogMergeTestSupport::makeStar(
        "h_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0
    );
    variedBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *variedBridgeRow.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 25.0,
        .stellarParallaxMas = 5.0,
        .radialVelocityKmPerSecond = -5.5,
    };
    OwnGalaxyCelestialBody variedMeasuredRow = CatalogMergeTestSupport::makeStar(
        "h_hip_99", {}, {CatalogIdentifier::make("hip", "99")}, {}, std::nullopt, std::nullopt
    );
    variedMeasuredRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .properMotionRightAscensionMasPerYear = 10.0,
        .stellarParallaxMas = 2.0,
        .radialVelocityKmPerSecond = 12.0,
    };
    OwnGalaxyCelestialBody variedLaterBridgeRow = CatalogMergeTestSupport::makeStar(
        "i_later_bridge", {}, {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
    );
    variedLaterBridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *variedLaterBridgeRow.fixedEquatorial,
        .stellarParallaxMas = 9.0,
    };
    const auto variedSourceA = CatalogMergeTestSupport::createCatalog(
        {variedFixedRow, variedSecondFixedRow, variedBridgeRow, variedMeasuredRow}, {}
    );
    const auto variedSourceB = CatalogMergeTestSupport::createCatalog({variedLaterBridgeRow}, {});
    QVERIFY(variedSourceA != nullptr);
    QVERIFY(variedSourceB != nullptr);

    const CatalogCompositionResult varied =
        CatalogMergeTestSupport::composeAll({variedSourceA.get(), variedSourceB.get()});

    QVERIFY(varied.isSuccess());
    QCOMPARE(varied.bodyCount, std::size_t{1});
    const BaseCelestialBody* variedSurvivor =
        CatalogMergeTestSupport::findBodyById(varied.catalog->bodies(), "i_later_bridge");
    QVERIFY(variedSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(variedSurvivor->displayName), QStringLiteral("First"));
    QVERIFY(variedSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(variedSurvivor->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(variedSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& variedAstrometry = *variedSurvivor->starAstrometryValue();
    QVERIFY(variedAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*variedAstrometry.properMotionRightAscensionMasPerYear, 25.0);
    QVERIFY(variedAstrometry.stellarParallaxMas.has_value());
    QCOMPARE(*variedAstrometry.stellarParallaxMas, 9.0);
    QVERIFY(variedAstrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*variedAstrometry.radialVelocityKmPerSecond, -5.5);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hip", "17"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*variedSurvivor, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "h_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "h_hip_17"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "h_hd_27"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*variedSurvivor, "h_hip_99"));
    QCOMPARE(varied.sourceIds.front(), std::string("source-1"));
    QCOMPARE(varied.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));
}

QTEST_APPLESS_MAIN(CatalogModelReplacementTests)

#include "CatalogModelReplacementTests.moc"
