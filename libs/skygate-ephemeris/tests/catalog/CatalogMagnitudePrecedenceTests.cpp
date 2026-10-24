#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "DistantCelestialBody.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogMagnitudePrecedenceTests final : public QObject {
    Q_OBJECT

private slots:
    void bridgeWithinOneSourceResolvesMagnitudeByRow();
    void zeroAndNegativeMagnitudesKeepRowPrecedence();
    void bridgedDeepSkyMagnitudesFillOnlyMissingValues();
    void replacementMagnitudeKeepsSourcePrecedence();
    void samePassDuplicateInheritsEarlierMagnitude();
};

void CatalogMagnitudePrecedenceTests::bridgeWithinOneSourceResolvesMagnitudeByRow()
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

void CatalogMagnitudePrecedenceTests::zeroAndNegativeMagnitudesKeepRowPrecedence()
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

void CatalogMagnitudePrecedenceTests::bridgedDeepSkyMagnitudesFillOnlyMissingValues()
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

void CatalogMagnitudePrecedenceTests::replacementMagnitudeKeepsSourcePrecedence()
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

void CatalogMagnitudePrecedenceTests::samePassDuplicateInheritsEarlierMagnitude()
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

QTEST_APPLESS_MAIN(CatalogMagnitudePrecedenceTests)

#include "CatalogMagnitudePrecedenceTests.moc"
