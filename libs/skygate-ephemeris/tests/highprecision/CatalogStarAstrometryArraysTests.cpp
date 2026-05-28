#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] AstronomicalEpoch referenceEpoch()
{
    return {
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.25,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] OwnGalaxyCelestialBody makeAstrometricStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "astrometric-star";
    body.displayName = "Astrometric Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 10.0,
        .declinationDeg = 20.0,
    };
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            {
                .rightAscensionHours = 10.25,
                .declinationDeg = 20.5,
            },
        .referenceEpoch = referenceEpoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
        .radialVelocityKmPerSecond = 21.0,
        .validityRange = EphemerisDateRange{
            .id = "test-validity",
            .displayName = "Test validity",
            .start = referenceEpoch(),
            .end = {
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.25,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makePartialAstrometricStar()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.id = "partial-star";
    body.starAstrometry->properMotionDeclinationMasPerYear = std::nullopt;
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    body.starAstrometry->radialVelocityKmPerSecond = std::nullopt;
    body.starAstrometry->validityRange = std::nullopt;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeFixedOnlyStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "fixed-star";
    body.displayName = "Fixed Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = -15.0,
    };
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makePlanet()
{
    OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] DistantCelestialBody makeFixedDeepSkyObject()
{
    DistantCelestialBody body;
    body.id = "messier-31";
    body.displayName = "M31";
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 0.7,
        .declinationDeg = 41.3,
    };
    body.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
    };
    return body;
}

[[nodiscard]] CatalogStarAstrometryArrays makeArrays(const std::vector<OwnGalaxyCelestialBody>& bodies)
{
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    return CatalogStarAstrometryArrays(catalog.bodies());
}

[[nodiscard]] OwnGalaxyCelestialBody makeStarWithInvalidNumericAstrometry()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.id = "invalid-astrometry-star";
    body.starAstrometry->properMotionRightAscensionMasPerYear = std::numeric_limits<double>::quiet_NaN();
    body.starAstrometry->properMotionDeclinationMasPerYear = std::numeric_limits<double>::infinity();
    body.starAstrometry->stellarParallaxMas = 0.0;
    body.starAstrometry->radialVelocityKmPerSecond = -std::numeric_limits<double>::infinity();
    return body;
}

}  // namespace

class CatalogStarAstrometryArraysTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsCacheFriendlyArraysFromFullPartialAndFixedStars();
    void batchesFixedCoordinateNonStarBodies();
    void masksOnlyUsableNumericAstrometryValues();
    void copiesCatalogDataAndSurvivesSourceLifetimeChanges();
};

void CatalogStarAstrometryArraysTests::buildsCacheFriendlyArraysFromFullPartialAndFixedStars()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makePlanet(),
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };

    const CatalogStarAstrometryArrays arrays = makeArrays(bodies);

    QCOMPARE(arrays.size(), 3U);
    QVERIFY(!arrays.empty());
    QCOMPARE(arrays.bodyIndices()[0], 1U);
    QCOMPARE(arrays.bodyIndices()[1], 2U);
    QCOMPARE(arrays.bodyIndices()[2], 3U);
    QVERIFY(!arrays.arrayIndexForBodyIndex(0).has_value());
    QCOMPARE(*arrays.arrayIndexForBodyIndex(2), 1U);

    QVERIFY(arrays.hasCatalogAstrometry(0));
    QVERIFY(arrays.hasFixedEquatorialFallback(0));
    QCOMPARE(arrays.referenceEquatorial(0).rightAscensionHours, 10.25);
    QCOMPARE(arrays.referenceEquatorial(0).declinationDeg, 20.5);
    QCOMPARE(arrays.referenceEpoch(0).julianDatePart1, referenceEpoch().julianDatePart1);
    QCOMPARE(static_cast<int>(arrays.referenceEpoch(0).timeScale), static_cast<int>(TimeScale::Tt));
    QCOMPARE(*arrays.properMotionRightAscensionMasPerYear(0), 125.0);
    QCOMPARE(*arrays.properMotionDeclinationMasPerYear(0), -55.0);
    QCOMPARE(*arrays.stellarParallaxMas(0), 7.5);
    QCOMPARE(*arrays.radialVelocityKmPerSecond(0), 21.0);
    QCOMPARE(QString::fromStdString(arrays.validityRange(0)->id), QStringLiteral("test-validity"));

    QVERIFY(arrays.hasCatalogAstrometry(1));
    QVERIFY(arrays.properMotionRightAscensionMasPerYear(1).has_value());
    QVERIFY(!arrays.properMotionDeclinationMasPerYear(1).has_value());
    QVERIFY(!arrays.stellarParallaxMas(1).has_value());
    QVERIFY(!arrays.radialVelocityKmPerSecond(1).has_value());
    QVERIFY(!arrays.validityRange(1).has_value());

    QVERIFY(!arrays.hasCatalogAstrometry(2));
    QVERIFY(arrays.hasFixedEquatorialFallback(2));
    QVERIFY(!arrays.properMotionRightAscensionMasPerYear(2).has_value());
    QCOMPARE(arrays.referenceEquatorial(2).rightAscensionHours, 4.0);
    QCOMPARE(arrays.referenceEquatorial(2).declinationDeg, -15.0);
    QCOMPARE(arrays.fixedEquatorialFallback(2)->declinationDeg, -15.0);

    QCOMPARE(arrays.referenceRightAscensionHours().size(), arrays.size());
    QCOMPARE(arrays.referenceDeclinationDegrees().size(), arrays.size());
    QCOMPARE(arrays.referenceEpochJulianDatePart1().size(), arrays.size());
    QCOMPARE(arrays.referenceEpochJulianDatePart2().size(), arrays.size());
    QCOMPARE(arrays.referenceEpochTimeScales().size(), arrays.size());
    QCOMPARE(arrays.hasCatalogAstrometryMask().size(), arrays.size());
    QCOMPARE(arrays.hasCatalogAstrometryMask()[0], std::uint8_t{1});
    QCOMPARE(arrays.hasCatalogAstrometryMask()[2], std::uint8_t{0});
    QCOMPARE(arrays.hasFixedEquatorialFallbackMask().size(), arrays.size());
    QCOMPARE(arrays.fixedRightAscensionHours().size(), arrays.size());
    QCOMPARE(arrays.fixedDeclinationDegrees().size(), arrays.size());
    QCOMPARE(arrays.fixedRightAscensionHours()[2], 4.0);
    QCOMPARE(arrays.fixedDeclinationDegrees()[2], -15.0);
    QCOMPARE(arrays.hasProperMotionRightAscensionMask().size(), arrays.size());
    QCOMPARE(arrays.properMotionRightAscensionMasPerYearValues().size(), arrays.size());
    QCOMPARE(arrays.hasProperMotionRightAscensionMask()[0], std::uint8_t{1});
    QCOMPARE(arrays.properMotionRightAscensionMasPerYearValues()[0], 125.0);
    QCOMPARE(arrays.hasProperMotionDeclinationMask().size(), arrays.size());
    QCOMPARE(arrays.properMotionDeclinationMasPerYearValues().size(), arrays.size());
    QCOMPARE(arrays.hasStellarParallaxMask().size(), arrays.size());
    QCOMPARE(arrays.stellarParallaxMasValues().size(), arrays.size());
    QCOMPARE(arrays.hasRadialVelocityMask().size(), arrays.size());
    QCOMPARE(arrays.radialVelocityKmPerSecondValues().size(), arrays.size());
    QCOMPARE(arrays.hasValidityRangeMask().size(), arrays.size());
    QCOMPARE(arrays.validityRanges().size(), arrays.size());
}

void CatalogStarAstrometryArraysTests::batchesFixedCoordinateNonStarBodies()
{
    const CelestialBodyCatalog catalog(
        std::vector<OwnGalaxyCelestialBody>{
            makeFixedOnlyStar(),
        },
        std::vector<DistantCelestialBody>{
            makeFixedDeepSkyObject(),
        },
        std::vector<CelestialBodyCatalog::OrderEntry>{
            {.domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = 0U},
            {.domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 0U},
        }
    );
    const CatalogStarAstrometryArrays arrays(catalog.bodies());

    QCOMPARE(arrays.size(), 2U);
    QCOMPARE(arrays.bodyIndices()[0], 0U);
    QCOMPARE(arrays.bodyIndices()[1], 1U);
    QCOMPARE(*arrays.arrayIndexForBodyIndex(0), 0U);
    QCOMPARE(*arrays.arrayIndexForBodyIndex(1), 1U);
    QVERIFY(!arrays.hasCatalogAstrometry(0));
    QVERIFY(arrays.hasFixedEquatorialFallback(0));
    QCOMPARE(arrays.referenceEquatorial(0).rightAscensionHours, 0.7);
    QCOMPARE(arrays.referenceEquatorial(0).declinationDeg, 41.3);
}

void CatalogStarAstrometryArraysTests::masksOnlyUsableNumericAstrometryValues()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makeStarWithInvalidNumericAstrometry(),
    };

    const CatalogStarAstrometryArrays arrays = makeArrays(bodies);

    QCOMPARE(arrays.size(), 1U);
    QCOMPARE(arrays.hasProperMotionRightAscensionMask()[0], std::uint8_t{0});
    QCOMPARE(arrays.hasProperMotionDeclinationMask()[0], std::uint8_t{0});
    QCOMPARE(arrays.hasStellarParallaxMask()[0], std::uint8_t{0});
    QCOMPARE(arrays.hasRadialVelocityMask()[0], std::uint8_t{0});
    QVERIFY(!arrays.properMotionRightAscensionMasPerYear(0).has_value());
    QVERIFY(!arrays.properMotionDeclinationMasPerYear(0).has_value());
    QVERIFY(!arrays.stellarParallaxMas(0).has_value());
    QVERIFY(!arrays.radialVelocityKmPerSecond(0).has_value());
    QVERIFY(std::isnan(arrays.properMotionRightAscensionMasPerYearValues()[0]));
    QVERIFY(std::isinf(arrays.properMotionDeclinationMasPerYearValues()[0]));
    QCOMPARE(arrays.stellarParallaxMasValues()[0], 0.0);
    QVERIFY(std::isinf(arrays.radialVelocityKmPerSecondValues()[0]));
}

void CatalogStarAstrometryArraysTests::copiesCatalogDataAndSurvivesSourceLifetimeChanges()
{
    CatalogStarAstrometryArrays arrays;
    {
        std::vector<OwnGalaxyCelestialBody> bodies{
            makeAstrometricStar(),
        };
        arrays = makeArrays(bodies);
        bodies[0].starAstrometry->referenceEquatorial.rightAscensionHours = 1.0;
        bodies[0].fixedEquatorial->declinationDeg = 88.0;
        bodies.clear();
    }

    QCOMPARE(arrays.size(), 1U);
    QCOMPARE(arrays.referenceEquatorial(0).rightAscensionHours, 10.25);
    QCOMPARE(arrays.fixedEquatorialFallback(0)->declinationDeg, 20.0);
    QCOMPARE(*arrays.properMotionRightAscensionMasPerYear(0), 125.0);
}

QTEST_APPLESS_MAIN(CatalogStarAstrometryArraysTests)

#include "CatalogStarAstrometryArraysTests.moc"
