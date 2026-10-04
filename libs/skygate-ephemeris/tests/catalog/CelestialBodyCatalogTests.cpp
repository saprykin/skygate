#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogLoader.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <span>
#include <vector>

namespace {

using skygate::core::AstronomicalEpoch;
using skygate::core::EquatorialCoordinate;
using skygate::core::TimeScale;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::CelestialBodyCatalog;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::EphemerisDateRange;
using skygate::ephemeris::OwnGalaxyCelestialBody;

[[nodiscard]] AstronomicalEpoch referenceEpoch()
{
    return {
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.0,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] OwnGalaxyCelestialBody makeStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "hip_1";
    body.displayName = "Alpha Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.visualMagnitude = 1.5;
    body.fixedEquatorial = EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = -16.0};
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 6.25, .declinationDeg = -16.5},
        .referenceEpoch = referenceEpoch(),
        .properMotionRightAscensionMasPerYear = 125.0,
        .properMotionDeclinationMasPerYear = -55.0,
        .stellarParallaxMas = 7.5,
        .radialVelocityKmPerSecond = 21.0,
        .validityRange = EphemerisDateRange{
            .id = "test-validity",
            .displayName = "Test validity",
            .start = referenceEpoch(),
            .end = AstronomicalEpoch{
                .julianDatePart1 = 2'452'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tt,
            },
        },
    };
    return body;
}

[[nodiscard]] DistantCelestialBody makePrimaryDeepSkyObject()
{
    DistantCelestialBody body;
    body.id = "ngc_224";
    body.displayName = "Andromeda";
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = 3.4;
    body.fixedEquatorial = EquatorialCoordinate{.rightAscensionHours = 0.7, .declinationDeg = 41.3};
    body.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"M31", "NGC 224"},
        .majorAxisArcmin = 178.0,
        .minorAxisArcmin = 63.0,
        .positionAngleDeg = 35.0,
    };
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeConstellation()
{
    OwnGalaxyCelestialBody body;
    body.id = "orion";
    body.displayName = "Orion";
    body.kind = BaseCelestialBody::Kind::Constellation;
    return body;
}

void verifyStar(const BaseCelestialBody& body)
{
    QCOMPARE(QString::fromStdString(body.id), QStringLiteral("hip_1"));
    QCOMPARE(QString::fromStdString(body.displayName), QStringLiteral("Alpha Star"));
    QCOMPARE(body.kind, BaseCelestialBody::Kind::Star);
    QCOMPARE(body.visualMagnitude, 1.5);

    QVERIFY(body.fixedEquatorialValue().has_value());
    QCOMPARE(body.fixedEquatorialValue()->rightAscensionHours, 6.0);
    QCOMPARE(body.fixedEquatorialValue()->declinationDeg, -16.0);

    QVERIFY(body.starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *body.starAstrometryValue();
    QCOMPARE(astrometry.referenceEquatorial.rightAscensionHours, 6.25);
    QCOMPARE(astrometry.referenceEquatorial.declinationDeg, -16.5);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart1, referenceEpoch().julianDatePart1);
    QCOMPARE(astrometry.referenceEpoch.julianDatePart2, referenceEpoch().julianDatePart2);
    QCOMPARE(static_cast<int>(astrometry.referenceEpoch.timeScale), static_cast<int>(TimeScale::Tt));
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 125.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -55.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 7.5);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, 21.0);
    QVERIFY(astrometry.validityRange.has_value());
    QCOMPARE(QString::fromStdString(astrometry.validityRange->id), QStringLiteral("test-validity"));
}

void verifyPrimaryDeepSkyObject(const BaseCelestialBody& body)
{
    QCOMPARE(QString::fromStdString(body.id), QStringLiteral("ngc_224"));
    QCOMPARE(QString::fromStdString(body.displayName), QStringLiteral("Andromeda"));
    QCOMPARE(body.kind, BaseCelestialBody::Kind::DeepSkyObject);
    QCOMPARE(body.visualMagnitude, 3.4);

    QVERIFY(body.fixedEquatorialValue().has_value());
    QCOMPARE(body.fixedEquatorialValue()->rightAscensionHours, 0.7);
    QCOMPARE(body.fixedEquatorialValue()->declinationDeg, 41.3);

    QVERIFY(body.deepSkyObjectValue().has_value());
    const DeepSkyObjectInfo& info = *body.deepSkyObjectValue();
    QCOMPARE(info.kind, DeepSkyObjectInfo::Kind::Galaxy);
    QCOMPARE(info.aliases.size(), std::size_t(2));
    QCOMPARE(QString::fromStdString(info.aliases[0]), QStringLiteral("M31"));
    QCOMPARE(QString::fromStdString(info.aliases[1]), QStringLiteral("NGC 224"));
    QVERIFY(info.majorAxisArcmin.has_value());
    QCOMPARE(*info.majorAxisArcmin, 178.0);
    QVERIFY(info.minorAxisArcmin.has_value());
    QCOMPARE(*info.minorAxisArcmin, 63.0);
    QVERIFY(info.positionAngleDeg.has_value());
    QCOMPARE(*info.positionAngleDeg, 35.0);
}

}  // namespace

class CelestialBodyCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void snapshotConstructionPreservesMixedBodiesAndOrder();
    void appendBodyPreservesFieldsAndOrder();
    void loaderSelectionPreservesAstrometry();
};

void CelestialBodyCatalogTests::snapshotConstructionPreservesMixedBodiesAndOrder()
{
    const OwnGalaxyCelestialBody star = makeStar();
    const DistantCelestialBody primaryDeepSkyObject = makePrimaryDeepSkyObject();
    const OwnGalaxyCelestialBody constellation = makeConstellation();

    const std::vector<const BaseCelestialBody*> bodies{&star, &primaryDeepSkyObject, &constellation};
    const CelestialBodyCatalog catalog(bodies);

    QCOMPARE(catalog.size(), std::size_t(3));
    QCOMPARE(catalog.ownGalaxyBodies().size(), std::size_t(2));
    QCOMPARE(catalog.distantBodies().size(), std::size_t(1));

    const std::span<const BaseCelestialBody* const> ordered = catalog.bodies();
    QCOMPARE(ordered.size(), std::size_t(3));
    verifyStar(*ordered[0]);
    verifyPrimaryDeepSkyObject(*ordered[1]);
    QCOMPARE(QString::fromStdString(ordered[2]->id), QStringLiteral("orion"));
    QCOMPARE(ordered[2]->kind, BaseCelestialBody::Kind::Constellation);
}

void CelestialBodyCatalogTests::appendBodyPreservesFieldsAndOrder()
{
    const OwnGalaxyCelestialBody star = makeStar();
    const DistantCelestialBody primaryDeepSkyObject = makePrimaryDeepSkyObject();
    const OwnGalaxyCelestialBody constellation = makeConstellation();

    CelestialBodyCatalog catalog;
    catalog.appendBody(star);
    catalog.appendBody(primaryDeepSkyObject);
    catalog.appendBody(constellation);

    QCOMPARE(catalog.size(), std::size_t(3));
    QCOMPARE(catalog.ownGalaxyBodies().size(), std::size_t(2));
    QCOMPARE(catalog.distantBodies().size(), std::size_t(1));

    const std::span<const BaseCelestialBody* const> ordered = catalog.bodies();
    verifyStar(*ordered[0]);
    verifyPrimaryDeepSkyObject(*ordered[1]);
    QCOMPARE(QString::fromStdString(ordered[2]->id), QStringLiteral("orion"));
    QCOMPARE(ordered[2]->kind, BaseCelestialBody::Kind::Constellation);
}

void CelestialBodyCatalogTests::loaderSelectionPreservesAstrometry()
{
    const skygate::ephemeris::CatalogLoadResult result = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag,pmra,pmdec,parallax,rv\n"
        "1,11,Alpha,1.0,2.0,3.0,10.0,-20.0,50.0,15.0\n"
        "2,22,Beta,4.0,5.0,1.0,12.0,-18.0,45.0,10.0\n"
        "3,33,Gamma,7.0,8.0,2.0,9.0,-11.0,60.0,5.0\n",
        {},
        skygate::ephemeris::CatalogSelectionOptions{
            .mode = skygate::ephemeris::CatalogSelectionMode::BrightestByVisualMagnitude, .maxBodyCount = 2U
        }
    );

    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const std::span<const BaseCelestialBody* const> ordered = result.catalog->bodies();
    QCOMPARE(ordered.size(), std::size_t(2));
    QCOMPARE(QString::fromStdString(ordered[0]->id), QStringLiteral("hip_22"));
    QCOMPARE(QString::fromStdString(ordered[1]->id), QStringLiteral("hip_33"));

    QCOMPARE(ordered[0]->visualMagnitude, 1.0);
    QVERIFY(ordered[0]->fixedEquatorialValue().has_value());
    QCOMPARE(ordered[0]->fixedEquatorialValue()->rightAscensionHours, 4.0);
    QCOMPARE(ordered[0]->fixedEquatorialValue()->declinationDeg, 5.0);
    QVERIFY(ordered[0]->starAstrometryValue().has_value());
    const CatalogStarAstrometry& astrometry = *ordered[0]->starAstrometryValue();
    QVERIFY(astrometry.properMotionRightAscensionMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionRightAscensionMasPerYear, 12.0);
    QVERIFY(astrometry.properMotionDeclinationMasPerYear.has_value());
    QCOMPARE(*astrometry.properMotionDeclinationMasPerYear, -18.0);
    QVERIFY(astrometry.stellarParallaxMas.has_value());
    QCOMPARE(*astrometry.stellarParallaxMas, 45.0);
    QVERIFY(astrometry.radialVelocityKmPerSecond.has_value());
    QCOMPARE(*astrometry.radialVelocityKmPerSecond, 10.0);
}

QTEST_APPLESS_MAIN(CelestialBodyCatalogTests)

#include "CelestialBodyCatalogTests.moc"
