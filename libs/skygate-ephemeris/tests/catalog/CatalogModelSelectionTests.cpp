#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <cstddef>

using skygate::core::AstronomicalEpoch;
using skygate::core::EquatorialCoordinate;
using skygate::core::TimeScale;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::EphemerisDateRange;
using skygate::ephemeris::OwnGalaxyCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogModelSelectionTests final : public QObject {
    Q_OBJECT

private slots:
    void keepsWinnerCoordinatesOverConflictingAstrometry();
    void rejectsLosingAstrometryThatContradictsTheWinningFixedPosition();
    void keepsOneCoherentCoordinateModelWhenConflictingSourcesAreReordered();
    void adoptsCompatibleLosingAstrometryForAFixedPositionWinner();
    void fillsMissingAstrometryFieldsAcrossCompatibleEpochs();
    void fillsMissingAstrometryFieldsFromAnUndeclaredEpoch();
    void rejectsLosingAstrometryThatDisagreesAfterEpochConversion();
    void rejectsLosingFixedCoordinatesThatContradictWinningAstrometry();
    void earliestRowModelWinsAcrossALocalBridge();
    void ordinaryDuplicateRowsKeepTheFirstRowsCoherentModel();
    void reversedRowsAndALocalBridgeKeepTheFirstRowsModel();
    void referenceOnlyAstrometryAnchorsTheEarliestRowModel();
    void epochCompatibilityDecidesTheOptionalFieldsOfTheEarliestModel();
    void adoptsAContradictoryDonorsFixedPositionWithoutItsAstrometry();
};

void CatalogModelSelectionTests::keepsWinnerCoordinatesOverConflictingAstrometry()
{
    const auto source = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("hip_1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0),
            CatalogMergeTestSupport::makeStar("hip_1_dup", {}, {CatalogIdentifier::make("hip", "1")}, {}, 5.0, 6.0),
        },
        {}
    );
    QVERIFY(source != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "hip_1"), std::size_t{1});

    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "hip_1");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->fixedEquatorialValue().has_value());
    QCOMPARE(merged->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(merged->fixedEquatorialValue()->declinationDeg, 2.0);
}

void CatalogModelSelectionTests::rejectsLosingAstrometryThatContradictsTheWinningFixedPosition()
{
    // The earlier source places the shared HIP 1 object at RA 1 with
    // astrometry at the same default reference epoch. The later source fixes
    // the object at RA 10 and carries no astrometry. The later source wins the
    // position, so its fixed coordinates survive and the unrelated astrometry
    // is rejected instead of being copied beside them.
    OwnGalaxyCelestialBody earlier =
        CatalogMergeTestSupport::makeStar("star_low", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    earlier.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *earlier.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody later =
        CatalogMergeTestSupport::makeStar("star_high", {}, {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0);

    const auto earlierSource = CatalogMergeTestSupport::createCatalog({earlier}, {});
    const auto laterSource = CatalogMergeTestSupport::createCatalog({later}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_high over conflicting fixed coordinates from star_low."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_high and rejected the incompatible astrometry of "
        "star_low."
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*earlierSource, *laterSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "star_high"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "star_low"), std::size_t{0});

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_high");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 20.0);
    QVERIFY(!winner->starAstrometryValue().has_value());
}

void CatalogModelSelectionTests::keepsOneCoherentCoordinateModelWhenConflictingSourcesAreReordered()
{
    // The same two records in the opposite collection order: the later source
    // now owns the coherent fixed plus astrometry model, so it wins whole and
    // only the losing fixed coordinates are rejected. Reordering sources never
    // publishes a survivor whose fixed position and reference position
    // disagree.
    OwnGalaxyCelestialBody astrometric =
        CatalogMergeTestSupport::makeStar("star_low", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0);
    astrometric.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *astrometric.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody fixedOnly =
        CatalogMergeTestSupport::makeStar("star_high", {}, {CatalogIdentifier::make("hip", "1")}, {}, 10.0, 20.0);

    const auto fixedSource = CatalogMergeTestSupport::createCatalog({fixedOnly}, {});
    const auto astrometricSource = CatalogMergeTestSupport::createCatalog({astrometric}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of star_low over conflicting fixed coordinates from star_high."
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*fixedSource, *astrometricSource);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_low");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, 2.0);
    QVERIFY(winner->starAstrometryValue().has_value());
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(*winner->starAstrometryValue()->properMotionRightAscensionMasPerYear, 125.0);
}

void CatalogModelSelectionTests::adoptsCompatibleLosingAstrometryForAFixedPositionWinner()
{
    // The winner's fixed position and the losing reference position agree, so
    // the losing astrometry enriches the survivor instead of being dropped.
    OwnGalaxyCelestialBody donor =
        CatalogMergeTestSupport::makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "2")}, {}, 5.0, -10.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *donor.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
        .radialVelocityKmPerSecond = -5.5,
        .validityRange = EphemerisDateRange{
            .id = "donor-validity",
            .displayName = "Donor validity",
            .start = CatalogMergeTestSupport::j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody fixedOnly =
        CatalogMergeTestSupport::makeStar("star_fixed", {}, {CatalogIdentifier::make("hip", "2")}, {}, 5.0, -10.0);

    const auto donorSource = CatalogMergeTestSupport::createCatalog({donor}, {});
    const auto fixedSource = CatalogMergeTestSupport::createCatalog({fixedOnly}, {});

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*donorSource, *fixedSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_fixed");
    QVERIFY(winner != nullptr);
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QCOMPARE(winner->fixedEquatorialValue()->declinationDeg, -10.0);

    QVERIFY(winner->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *winner->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, -10.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, CatalogMergeTestSupport::j2000Epoch().julianDatePart1);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart2, CatalogMergeTestSupport::j2000Epoch().julianDatePart2);
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

void CatalogModelSelectionTests::fillsMissingAstrometryFieldsAcrossCompatibleEpochs()
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

    OwnGalaxyCelestialBody donor =
        CatalogMergeTestSupport::makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            EquatorialCoordinate{.rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = donorDecOffsetDeg},
        .referenceEpoch =
            AstronomicalEpoch{
                .julianDatePart1 = CatalogMergeTestSupport::j2000Epoch().julianDatePart1,
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
            .start = CatalogMergeTestSupport::j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody winner =
        CatalogMergeTestSupport::makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = kProperMotionRaMasPerYear,
    };

    const auto donorSource = CatalogMergeTestSupport::createCatalog({donor}, {});
    const auto winnerSource = CatalogMergeTestSupport::createCatalog({winner}, {});

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_winner");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *merged->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 0.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, CatalogMergeTestSupport::j2000Epoch().julianDatePart1);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart2, CatalogMergeTestSupport::j2000Epoch().julianDatePart2);
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

void CatalogModelSelectionTests::fillsMissingAstrometryFieldsFromAnUndeclaredEpoch()
{
    // A reference position that declares no epoch claims the same epoch as the
    // winner's declared reference epoch, so no epoch conversion applies and
    // agreeing positions still enrich the winner.
    OwnGalaxyCelestialBody donor =
        CatalogMergeTestSupport::makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "5")}, {}, 2.0, 3.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *donor.fixedEquatorial,
        .properMotionRightAscensionMasPerYear = 500.0,
        .stellarParallaxMas = 6.0,
        .radialVelocityKmPerSecond = -3.0,
    };
    OwnGalaxyCelestialBody winner =
        CatalogMergeTestSupport::makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "5")}, {}, 2.0, 3.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 250.0,
    };

    const auto donorSource = CatalogMergeTestSupport::createCatalog({donor}, {});
    const auto winnerSource = CatalogMergeTestSupport::createCatalog({winner}, {});

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_winner");
    QVERIFY(merged != nullptr);
    QVERIFY(merged->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *merged->starAstrometryValue();
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, CatalogMergeTestSupport::j2000Epoch().julianDatePart1);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 250.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 6.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, -3.0);
}

void CatalogModelSelectionTests::rejectsLosingAstrometryThatDisagreesAfterEpochConversion()
{
    // Same epoch gap and reference offset as the compatible case, but the
    // losing proper motion is twice the rate the offset implies. The epoch
    // conversion leaves a 50 arcsecond residual, so the models describe
    // different directions and the losing astrometry does not enrich the
    // winner.
    constexpr double kEpochGapYears = 50.0;
    constexpr double kImpliedProperMotionRaMasPerYear = 1'000.0;
    const double donorRaOffsetHours = kImpliedProperMotionRaMasPerYear * kEpochGapYears / 3'600'000.0 / 15.0;

    OwnGalaxyCelestialBody donor =
        CatalogMergeTestSupport::makeStar("star_donor", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    donor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            EquatorialCoordinate{.rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = 0.0},
        .referenceEpoch =
            AstronomicalEpoch{
                .julianDatePart1 = CatalogMergeTestSupport::j2000Epoch().julianDatePart1,
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
            .start = CatalogMergeTestSupport::j2000Epoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    OwnGalaxyCelestialBody winner =
        CatalogMergeTestSupport::makeStar("star_winner", {}, {CatalogIdentifier::make("hip", "3")}, {}, 1.0, 0.0);
    winner.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *winner.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = kImpliedProperMotionRaMasPerYear,
    };

    const auto donorSource = CatalogMergeTestSupport::createCatalog({donor}, {});
    const auto winnerSource = CatalogMergeTestSupport::createCatalog({winner}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of star_winner and rejected the incompatible astrometry of "
        "star_donor."
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*donorSource, *winnerSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* merged = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_winner");
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

void CatalogModelSelectionTests::rejectsLosingFixedCoordinatesThatContradictWinningAstrometry()
{
    // The winner (later source) carries astrometry at RA 1 and no fixed
    // position. The losing record's fixed position at RA 10 is unrelated, so it
    // is rejected instead of contradicting the winning reference coordinates.
    OwnGalaxyCelestialBody fixedOnly =
        CatalogMergeTestSupport::makeStar("star_fixed", {}, {CatalogIdentifier::make("hip", "4")}, {}, 10.0, 20.0);
    OwnGalaxyCelestialBody astrometric =
        CatalogMergeTestSupport::makeStar("star_astrometric", {}, {CatalogIdentifier::make("hip", "4")});
    astrometric.fixedEquatorial.reset();
    astrometric.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
    };

    const auto fixedSource = CatalogMergeTestSupport::createCatalog({fixedOnly}, {});
    const auto astrometricSource = CatalogMergeTestSupport::createCatalog({astrometric}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the reference coordinates of star_astrometric and rejected the incompatible fixed "
        "coordinates of star_fixed."
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeInOrder(*fixedSource, *astrometricSource);

    QVERIFY(result.isSuccess());
    const BaseCelestialBody* winner =
        CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "star_astrometric");
    QVERIFY(winner != nullptr);
    QVERIFY(!winner->fixedEquatorialValue().has_value());
    QVERIFY(winner->starAstrometryValue().has_value());
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(winner->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
}

void CatalogModelSelectionTests::earliestRowModelWinsAcrossALocalBridge()
{
    // HIP 17 and HD 27 each describe the object at RA 1 hour with a matching
    // reference position at the same epoch, while the bridge describes RA 5
    // hours in both representations. The first row's coherent model must win
    // as a whole: the bridge supplies the public canonical id, not the model.
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
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    const auto source = CatalogMergeTestSupport::createCatalog({hipRow, hdRow, bridgeRow}, {});

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
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(
        survivor->starAstrometryValue()->referenceEpoch.julianDatePart1,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart1
    );
    QCOMPARE(
        survivor->starAstrometryValue()->referenceEpoch.julianDatePart2,
        CatalogMergeTestSupport::j2000Epoch().julianDatePart2
    );
}

void CatalogModelSelectionTests::ordinaryDuplicateRowsKeepTheFirstRowsCoherentModel()
{
    // Two rows of one source place the object at RA 1 and RA 5 with internally
    // consistent models. The first row is authoritative within its source, so
    // a duplicate never replaces its model.
    OwnGalaxyCelestialBody firstRow =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody duplicateRow = CatalogMergeTestSupport::makeStar(
        "hip_17_copy", "Second", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0
    );
    duplicateRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *duplicateRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    const auto source = CatalogMergeTestSupport::createCatalog({firstRow, duplicateRow}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "hip_17");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 1.0);
}

void CatalogModelSelectionTests::reversedRowsAndALocalBridgeKeepTheFirstRowsModel()
{
    // The first row declares RA 5 and the second row RA 1, and the bridge
    // declares RA 5 while absorbing both. The first row's model still wins as
    // a whole, so the later row's coherent RA 1 model is not mixed into it.
    OwnGalaxyCelestialBody firstRow =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 5.0, 2.0);
    firstRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *firstRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody secondRow =
        CatalogMergeTestSupport::makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
    secondRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *secondRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    const auto source = CatalogMergeTestSupport::createCatalog({firstRow, secondRow, bridgeRow}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(survivor->fixedEquatorialValue().has_value());
    QCOMPARE(survivor->fixedEquatorialValue()->rightAscensionHours, 5.0);
    QVERIFY(survivor->starAstrometryValue().has_value());
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.rightAscensionHours, 5.0);
    QCOMPARE(survivor->starAstrometryValue()->referenceEquatorial.declinationDeg, 2.0);
}

void CatalogModelSelectionTests::referenceOnlyAstrometryAnchorsTheEarliestRowModel()
{
    // The first two rows carry only a reference position and epoch, with no
    // fixed position and no optional fields, while the bridge carries a full
    // model at RA 5. The earliest row's reference-only model still anchors the
    // survivor, so model selection cannot depend on optional-field presence.
    // The second row agrees and enriches the selected model with its proper
    // motion and parallax.
    OwnGalaxyCelestialBody hipRow = CatalogMergeTestSupport::makeStar(
        "hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, std::nullopt, std::nullopt
    );
    hipRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
    };
    OwnGalaxyCelestialBody hdRow = CatalogMergeTestSupport::makeStar(
        "hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt
    );
    hdRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 30.0,
        .properMotionDeclinationMasPerYear = -10.0,
        .stellarParallaxMas = 5.0,
    };
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        5.0,
        2.0
    );
    bridgeRow.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *bridgeRow.fixedEquatorial,
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
    };
    const auto source = CatalogMergeTestSupport::createCatalog({hipRow, hdRow, bridgeRow}, {});

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

    const CatalogCompositionResult result = CatalogMergeTestSupport::composePrimary(*source);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "bridge_last");
    QVERIFY(survivor != nullptr);
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("First"));
    QVERIFY(!survivor->fixedEquatorialValue().has_value());
    QVERIFY(survivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *survivor->starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, CatalogMergeTestSupport::j2000Epoch().julianDatePart1);
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 30.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -10.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 5.0);
}

void CatalogModelSelectionTests::epochCompatibilityDecidesTheOptionalFieldsOfTheEarliestModel()
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
        OwnGalaxyCelestialBody row =
            CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
            .properMotionRightAscensionMasPerYear = kProperMotionRaMasPerYear,
        };
        return row;
    };
    const auto makeSecondRow = [&](const double properMotionRaMasPerYear) {
        OwnGalaxyCelestialBody row =
            CatalogMergeTestSupport::makeStar("hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, 1.0, 2.0);
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial =
                EquatorialCoordinate{
                    .rightAscensionHours = 1.0 + donorRaOffsetHours, .declinationDeg = 2.0 + donorDecOffsetDeg
                },
            .referenceEpoch =
                AstronomicalEpoch{
                    .julianDatePart1 = CatalogMergeTestSupport::j2000Epoch().julianDatePart1,
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
        OwnGalaxyCelestialBody row = CatalogMergeTestSupport::makeStar(
            "bridge_last",
            "Last bridge",
            {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
            {},
            5.0,
            2.0
        );
        row.starAstrometry = CatalogStarAstrometry{
            .referenceEquatorial = *row.fixedEquatorial,
            .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        };
        return row;
    };

    const auto compatibleSource = CatalogMergeTestSupport::createCatalog(
        {makeEarliestRow(), makeSecondRow(kProperMotionRaMasPerYear), makeBridgeRow()}, {}
    );

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

    const CatalogCompositionResult compatible = CatalogMergeTestSupport::composePrimary(*compatibleSource);

    QVERIFY(compatible.isSuccess());
    QCOMPARE(compatible.bodyCount, std::size_t{1});
    const BaseCelestialBody* compatibleSurvivor =
        CatalogMergeTestSupport::findBodyById(compatible.catalog->bodies(), "bridge_last");
    QVERIFY(compatibleSurvivor != nullptr);
    QVERIFY(compatibleSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& compatibleAstrometry = *compatibleSurvivor->starAstrometryValue();
    QCOMPARE(compatibleAstrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(compatibleAstrometry.referenceEquatorial.declinationDeg, 2.0);
    QCOMPARE(
        compatibleAstrometry.referenceEpoch.julianDatePart1, CatalogMergeTestSupport::j2000Epoch().julianDatePart1
    );
    QCOMPARE(
        compatibleAstrometry.referenceEpoch.julianDatePart2, CatalogMergeTestSupport::j2000Epoch().julianDatePart2
    );
    QVERIFY(compatibleAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*compatibleAstrometry.properMotionRightAscensionMasPerYear, kProperMotionRaMasPerYear);
    QVERIFY(compatibleAstrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*compatibleAstrometry.properMotionDeclinationMasPerYear, kProperMotionDecMasPerYear);
    QVERIFY(compatibleAstrometry.stellarParallaxMas.has_value());
    QCOMPARE(*compatibleAstrometry.stellarParallaxMas, 5.0);

    // The same fixture with a proper motion twice the rate the reference
    // offset implies: the epoch conversion leaves a residual, so the second
    // row is rejected and the earliest row's model stays the only description.
    const auto incompatibleSource = CatalogMergeTestSupport::createCatalog(
        {makeEarliestRow(), makeSecondRow(2.0 * kProperMotionRaMasPerYear), makeBridgeRow()}, {}
    );

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

    const CatalogCompositionResult incompatible = CatalogMergeTestSupport::composePrimary(*incompatibleSource);

    QVERIFY(incompatible.isSuccess());
    QCOMPARE(incompatible.bodyCount, std::size_t{1});
    const BaseCelestialBody* incompatibleSurvivor =
        CatalogMergeTestSupport::findBodyById(incompatible.catalog->bodies(), "bridge_last");
    QVERIFY(incompatibleSurvivor != nullptr);
    QVERIFY(incompatibleSurvivor->starAstrometryValue().has_value());
    const CatalogStarAstrometry& incompatibleAstrometry = *incompatibleSurvivor->starAstrometryValue();
    QCOMPARE(incompatibleAstrometry.referenceEquatorial.rightAscensionHours, 1.0);
    QCOMPARE(
        incompatibleAstrometry.referenceEpoch.julianDatePart2, CatalogMergeTestSupport::j2000Epoch().julianDatePart2
    );
    QVERIFY(incompatibleAstrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*incompatibleAstrometry.properMotionRightAscensionMasPerYear, kProperMotionRaMasPerYear);
    QVERIFY(!incompatibleAstrometry.properMotionDeclinationMasPerYear.has_value());
    QVERIFY(!incompatibleAstrometry.stellarParallaxMas.has_value());
}

void CatalogModelSelectionTests::adoptsAContradictoryDonorsFixedPositionWithoutItsAstrometry()
{
    // The donor row fixes the object at RA 1 while its own astrometry anchors
    // it at RA 5. The bridge row carries no model of its own, so it adopts the
    // donor's model whole; the donor's fixed position survives and its
    // self-contradicting astrometry is rejected instead of accompanying it.
    OwnGalaxyCelestialBody contradictoryDonor =
        CatalogMergeTestSupport::makeStar("hip_17", "First", {CatalogIdentifier::make("hip", "17")}, {}, 1.0, 2.0);
    contradictoryDonor.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 5.0, .declinationDeg = 2.0},
        .referenceEpoch = CatalogMergeTestSupport::j2000Epoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
    };
    OwnGalaxyCelestialBody secondDonor = CatalogMergeTestSupport::makeStar(
        "hd_27", "Second", {CatalogIdentifier::make("hd", "27")}, {}, std::nullopt, std::nullopt
    );
    OwnGalaxyCelestialBody bridgeRow = CatalogMergeTestSupport::makeStar(
        "bridge_last",
        "Last bridge",
        {CatalogIdentifier::make("hip", "17"), CatalogIdentifier::make("hd", "27")},
        {},
        std::nullopt,
        std::nullopt
    );
    const auto source = CatalogMergeTestSupport::createCatalog({contradictoryDonor, secondDonor, bridgeRow}, {});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept the fixed coordinates of bridge_last and rejected the incompatible astrometry of "
        "hip_17."
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

QTEST_APPLESS_MAIN(CatalogModelSelectionTests)

#include "CatalogModelSelectionTests.moc"
