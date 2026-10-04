#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "catalog/IStarCatalog.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::core::AstronomicalEpoch;
using skygate::core::EquatorialCoordinate;
using skygate::core::TimeScale;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogSnapshotValidator;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::CelestialBodyCatalog;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

[[nodiscard]] AstronomicalEpoch referenceEpoch()
{
    return {
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.0,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] OwnGalaxyCelestialBody makeStar(std::string id)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = "Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return body;
}

[[nodiscard]] DistantCelestialBody makeDeepSkyObject(std::string id)
{
    DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = "DSO";
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.fixedEquatorial = EquatorialCoordinate{.rightAscensionHours = 0.7, .declinationDeg = 41.3};
    body.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"M31"},
        .majorAxisArcmin = 190.0,
        .minorAxisArcmin = 60.0,
    };
    return body;
}

[[nodiscard]] std::vector<CelestialBodyCatalog::OrderEntry>
sequentialOrder(const std::size_t ownGalaxyCount, const std::size_t distantCount)
{
    std::vector<CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyCount + distantCount);
    for (std::size_t index = 0; index < ownGalaxyCount; ++index) {
        order.push_back({.domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = index});
    }
    for (std::size_t index = 0; index < distantCount; ++index) {
        order.push_back({.domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = index});
    }
    return order;
}

[[nodiscard]] CatalogSnapshotValidator::Report validateVectors(
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies = {}
)
{
    const std::vector<CelestialBodyCatalog::OrderEntry> order =
        sequentialOrder(ownGalaxyBodies.size(), distantBodies.size());
    return CatalogSnapshotValidator::validate(ownGalaxyBodies, distantBodies, order);
}

}  // namespace

class CatalogSnapshotValidatorTests final : public QObject {
    Q_OBJECT

private slots:
    void vectorFactoryNormalizesSunAndMoonIdentities();
    void existingCatalogFactoryAcceptsValidSnapshot();
    void existingCatalogFactoryNormalizesSunAndIdentifiers();
    void validatorRejectsOutOfRangeAndUnknownOrderEntries();
    void validatorRejectsEmptyCanonicalIds();
    void validatorRejectsInvalidNumericDomains();
    void validatorAllowsAbsentOptionalMetadata();
    void cacheDeserializationRejectsInvalidNumericAndIdentityData();
};

void CatalogSnapshotValidatorTests::vectorFactoryNormalizesSunAndMoonIdentities()
{
    OwnGalaxyCelestialBody sun;
    sun.id = "SUN";
    sun.displayName = "Sun";
    sun.kind = BaseCelestialBody::Kind::Star;

    OwnGalaxyCelestialBody identifiedStar = makeStar("hip_1");
    identifiedStar.identity.externalIdentifiers.push_back(CatalogIdentifier{"HIP", "0031"});

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::vector<OwnGalaxyCelestialBody>{sun, identifiedStar}
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QCOMPARE(bodies.size(), std::size_t{2});
    QCOMPARE(bodies[0]->kind, BaseCelestialBody::Kind::Sun);
    QCOMPARE(bodies[1]->kind, BaseCelestialBody::Kind::Star);

    const BaseCelestialBody& normalizedStar = *bodies[1];
    QCOMPARE(normalizedStar.identity.externalIdentifiers.size(), std::size_t{1});
    QCOMPARE(
        QString::fromStdString(normalizedStar.identity.externalIdentifiers[0].namespaceName), QStringLiteral("hip")
    );
    QCOMPARE(QString::fromStdString(normalizedStar.identity.externalIdentifiers[0].value), QStringLiteral("31"));
}

void CatalogSnapshotValidatorTests::existingCatalogFactoryAcceptsValidSnapshot()
{
    const OwnGalaxyCelestialBody star = makeStar("hip_1");
    const DistantCelestialBody deepSkyObject = makeDeepSkyObject("ngc_224");
    const std::vector<const BaseCelestialBody*> bodies{&star, &deepSkyObject};

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromCatalog(CelestialBodyCatalog{bodies});
    QVERIFY(catalog != nullptr);
    QCOMPARE(catalog->bodies().size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(catalog->bodies()[0]->id), QStringLiteral("hip_1"));
    QCOMPARE(QString::fromStdString(catalog->bodies()[1]->id), QStringLiteral("ngc_224"));
}

void CatalogSnapshotValidatorTests::existingCatalogFactoryNormalizesSunAndIdentifiers()
{
    OwnGalaxyCelestialBody sun;
    sun.id = "SUN";
    sun.displayName = "Sun";
    sun.kind = BaseCelestialBody::Kind::Star;

    OwnGalaxyCelestialBody identifiedStar = makeStar("hip_1");
    identifiedStar.identity.externalIdentifiers.push_back(CatalogIdentifier{"HIP", "0031"});

    const std::vector<const BaseCelestialBody*> bodies{&sun, &identifiedStar};

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromCatalog(CelestialBodyCatalog{bodies});
    QVERIFY(catalog != nullptr);

    const auto normalizedBodies = catalog->bodies();
    QCOMPARE(normalizedBodies.size(), std::size_t{2});
    QCOMPARE(normalizedBodies[0]->kind, BaseCelestialBody::Kind::Sun);
    QCOMPARE(normalizedBodies[1]->kind, BaseCelestialBody::Kind::Star);

    const BaseCelestialBody& normalizedStar = *normalizedBodies[1];
    QCOMPARE(normalizedStar.identity.externalIdentifiers.size(), std::size_t{1});
    QCOMPARE(
        QString::fromStdString(normalizedStar.identity.externalIdentifiers[0].namespaceName), QStringLiteral("hip")
    );
    QCOMPARE(QString::fromStdString(normalizedStar.identity.externalIdentifiers[0].value), QStringLiteral("31"));
}

void CatalogSnapshotValidatorTests::validatorRejectsOutOfRangeAndUnknownOrderEntries()
{
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies{makeStar("hip_1")};
    std::vector<DistantCelestialBody> distantBodies;

    std::vector<CelestialBodyCatalog::OrderEntry> outOfRange{
        {.domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 1U},
    };
    const CatalogSnapshotValidator::Report outOfRangeReport =
        CatalogSnapshotValidator::validate(ownGalaxyBodies, distantBodies, outOfRange);
    QVERIFY(!outOfRangeReport.ok);
    QVERIFY(!outOfRangeReport.errorDetail.empty());

    std::vector<CelestialBodyCatalog::OrderEntry> unknownDomain{
        {.domain = static_cast<CelestialBodyCatalog::BodyDomain>(0xFF), .bodyIndex = 0U},
    };
    const CatalogSnapshotValidator::Report unknownDomainReport =
        CatalogSnapshotValidator::validate(ownGalaxyBodies, distantBodies, unknownDomain);
    QVERIFY(!unknownDomainReport.ok);

    const CatalogSnapshotValidator::Report validReport = validateVectors(std::move(ownGalaxyBodies));
    QVERIFY(validReport.ok);
}

void CatalogSnapshotValidatorTests::validatorRejectsEmptyCanonicalIds()
{
    OwnGalaxyCelestialBody body = makeStar("");
    const CatalogSnapshotValidator::Report report = validateVectors({std::move(body)});
    QVERIFY(!report.ok);
    QVERIFY(!report.errorDetail.empty());
}

void CatalogSnapshotValidatorTests::validatorRejectsInvalidNumericDomains()
{
    OwnGalaxyCelestialBody infiniteMagnitude = makeStar("hip_inf_mag");
    infiniteMagnitude.visualMagnitude = std::numeric_limits<double>::infinity();
    const CatalogSnapshotValidator::Report magnitudeReport = validateVectors({std::move(infiniteMagnitude)});
    QVERIFY(!magnitudeReport.ok);

    OwnGalaxyCelestialBody badCoordinate = makeStar("hip_bad_ra");
    badCoordinate.fixedEquatorial = EquatorialCoordinate{.rightAscensionHours = 25.0, .declinationDeg = 2.0};
    const CatalogSnapshotValidator::Report coordinateReport = validateVectors({std::move(badCoordinate)});
    QVERIFY(!coordinateReport.ok);

    OwnGalaxyCelestialBody negativeParallax = makeStar("hip_bad_parallax");
    negativeParallax.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        .referenceEpoch = referenceEpoch(),
        .stellarParallaxMas = -1.0,
    };
    const CatalogSnapshotValidator::Report parallaxReport = validateVectors({std::move(negativeParallax)});
    QVERIFY(!parallaxReport.ok);

    DistantCelestialBody negativeExtent = makeDeepSkyObject("ngc_negative");
    negativeExtent.deepSkyObject->majorAxisArcmin = -5.0;
    const CatalogSnapshotValidator::Report extentReport = validateVectors({}, {std::move(negativeExtent)});
    QVERIFY(!extentReport.ok);
}

void CatalogSnapshotValidatorTests::validatorAllowsAbsentOptionalMetadata()
{
    OwnGalaxyCelestialBody planet;
    planet.id = "mercury";
    planet.displayName = "Mercury";
    planet.kind = BaseCelestialBody::Kind::Planet;
    const CatalogSnapshotValidator::Report planetReport = validateVectors({std::move(planet)});
    QVERIFY(planetReport.ok);

    const CatalogSnapshotValidator::Report fixedOnlyStarReport = validateVectors({makeStar("hip_fixed")});
    QVERIFY(fixedOnlyStarReport.ok);

    DistantCelestialBody infoOnlyDso;
    infoOnlyDso.id = "ngc_info_only";
    infoOnlyDso.displayName = "Info Only";
    infoOnlyDso.kind = BaseCelestialBody::Kind::DeepSkyObject;
    infoOnlyDso.visualMagnitude = std::numeric_limits<double>::quiet_NaN();
    infoOnlyDso.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"Info Galaxy"},
    };
    const CatalogSnapshotValidator::Report dsoReport = validateVectors({}, {std::move(infoOnlyDso)});
    QVERIFY(dsoReport.ok);
}

void CatalogSnapshotValidatorTests::cacheDeserializationRejectsInvalidNumericAndIdentityData()
{
    std::vector<OwnGalaxyCelestialBody> infiniteMagnitudeBodies{makeStar("hip_inf_mag")};
    infiniteMagnitudeBodies[0].visualMagnitude = std::numeric_limits<double>::infinity();
    const CelestialBodyCatalog infiniteMagnitudeCatalog(
        std::move(infiniteMagnitudeBodies), std::vector<DistantCelestialBody>{}, sequentialOrder(1U, 0U)
    );
    const QByteArray infiniteMagnitudePayload =
        skygate::ephemeris::CatalogBinaryCodec::serialize(infiniteMagnitudeCatalog);
    QVERIFY(skygate::ephemeris::CatalogBinaryCodec::deserialize(infiniteMagnitudePayload) == nullptr);

    std::vector<OwnGalaxyCelestialBody> emptyIdBodies{makeStar("")};
    const CelestialBodyCatalog emptyIdCatalog(
        std::move(emptyIdBodies), std::vector<DistantCelestialBody>{}, sequentialOrder(1U, 0U)
    );
    const QByteArray emptyIdPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(emptyIdCatalog);
    QVERIFY(skygate::ephemeris::CatalogBinaryCodec::deserialize(emptyIdPayload) == nullptr);

    // Valid payloads still round-trip through the shared validation boundary.
    const CelestialBodyCatalog validCatalog(
        std::vector<OwnGalaxyCelestialBody>{makeStar("hip_1")},
        std::vector<DistantCelestialBody>{makeDeepSkyObject("ngc_224")},
        sequentialOrder(1U, 1U)
    );
    const QByteArray validPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(validCatalog);
    const std::unique_ptr<skygate::ephemeris::IStarCatalog> restored =
        skygate::ephemeris::CatalogBinaryCodec::deserialize(validPayload);
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->bodies().size(), std::size_t{2});
}

QTEST_APPLESS_MAIN(CatalogSnapshotValidatorTests)

#include "CatalogSnapshotValidatorTests.moc"
