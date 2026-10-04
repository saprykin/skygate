#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "TestHelpers.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/CatalogObjectIdentity.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogObjectIdentity;
using skygate::ephemeris::CelestialBodyCatalog;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

bool hasIdentifier(const CatalogObjectIdentity& identity, const std::string& namespaceName, const std::string& value)
{
    return std::any_of(
        identity.externalIdentifiers.begin(),
        identity.externalIdentifiers.end(),
        [&](const CatalogIdentifier& identifier) {
            return identifier.namespaceName == namespaceName && identifier.value == value;
        }
    );
}

bool hasAlias(const std::vector<std::string>& aliases, const std::string& alias)
{
    return std::find(aliases.begin(), aliases.end(), alias) != aliases.end();
}

OwnGalaxyCelestialBody makeStarWithIdentity()
{
    OwnGalaxyCelestialBody body;
    body.id = "hip_1";
    body.displayName = "Alpha";
    body.kind = BaseCelestialBody::Kind::Star;
    body.visualMagnitude = 2.0;
    body.identity.sourceRecordId = "1";
    body.identity.externalIdentifiers = {
        CatalogIdentifier::make("hip", "1"),
        CatalogIdentifier::make("hyg", "1"),
    };
    body.identity.aliases = {"Alpha Star"};
    return body;
}

DistantCelestialBody makeDeepSkyObjectWithIdentity()
{
    DistantCelestialBody body;
    body.id = "ngc_224";
    body.displayName = "Andromeda";
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = 3.4;
    body.identity.sourceRecordId = "NGC0224";
    body.identity.externalIdentifiers = {
        CatalogIdentifier::make("messier", "31"),
        CatalogIdentifier::make("ngc", "224"),
    };
    body.identity.aliases = {"M 31", "Andromeda Galaxy"};
    body.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"M 31", "Andromeda Galaxy"},
    };
    return body;
}

constexpr std::string_view kOpenNgcHeader = "Name;Type;RA;Dec;M;NGC;IC;Identifiers;Common names\n";

}  // namespace

class CatalogObjectIdentityTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizesIdentifiersPerNamespace();
    void hygParserPreservesCrossIdentifiersAndAliases();
    void hygParserFallsBackWithoutCrossIdentifiers();
    void openNgcParserPreservesMessierNgcIcIdentifiers();
    void bundledCatalogPreservesPlanetAndMessierIdentity();
    void copyPathsPreserveIdentity();
    void binaryRoundTripPreservesIdentity();
    void rejectsLegacyBinarySchemaVersion();
};

void CatalogObjectIdentityTests::normalizesIdentifiersPerNamespace()
{
    const CatalogIdentifier hip = CatalogIdentifier::make("hip", "032349");
    QCOMPARE(QString::fromStdString(hip.namespaceName), QStringLiteral("hip"));
    QCOMPARE(QString::fromStdString(hip.value), QStringLiteral("32349"));
    QCOMPARE(QString::fromStdString(hip.key()), QStringLiteral("hip:32349"));

    const CatalogIdentifier hyg = CatalogIdentifier::make("hyg", "32349");
    QVERIFY(hip != hyg);
    QCOMPARE(QString::fromStdString(hyg.key()), QStringLiteral("hyg:32349"));

    const CatalogIdentifier hipUpper = CatalogIdentifier::make("HIP", "032349");
    QVERIFY(hip == hipUpper);

    QCOMPARE(QString::fromStdString(CatalogIdentifier::make("messier", "31").value), QStringLiteral("031"));
    QCOMPARE(QString::fromStdString(CatalogIdentifier::make("messier", "001").value), QStringLiteral("001"));
    QCOMPARE(QString::fromStdString(CatalogIdentifier::make("ngc", "0224").value), QStringLiteral("224"));
    QCOMPARE(QString::fromStdString(CatalogIdentifier::make("ic", "04715").value), QStringLiteral("4715"));
}

void CatalogObjectIdentityTests::hygParserPreservesCrossIdentifiersAndAliases()
{
    const auto result = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,bf,ra,dec,mag\n"
        "1,32349,Sirius,9 CMa,6.7525,-16.7161,-1.46\n"
    );
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto bodies = result.catalog->bodies();
    const auto* body = skygate::ephemeris::tests::findBodyById(bodies, "hip_32349");
    QVERIFY(body != nullptr);
    QCOMPARE(QString::fromStdString(body->identity.sourceRecordId), QStringLiteral("1"));
    QCOMPARE(body->identity.idScope, CatalogObjectIdentity::IdScope::Global);
    QVERIFY(hasIdentifier(body->identity, "hip", "32349"));
    QVERIFY(hasIdentifier(body->identity, "hyg", "1"));
    QVERIFY(hasAlias(body->identity.aliases, "Sirius"));
    QVERIFY(hasAlias(body->identity.aliases, "9 CMa"));
}

void CatalogObjectIdentityTests::hygParserFallsBackWithoutCrossIdentifiers()
{
    const auto result = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        ",,Unnamed,1.0,2.0,3.0\n"
    );
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto bodies = result.catalog->bodies();
    const auto* body = skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1");
    QVERIFY(body != nullptr);
    QVERIFY(body->identity.sourceRecordId.empty());
    QVERIFY(body->identity.externalIdentifiers.empty());
    QCOMPARE(body->identity.idScope, CatalogObjectIdentity::IdScope::SourceLocal);
    QVERIFY(hasAlias(body->identity.aliases, "Unnamed"));
}

void CatalogObjectIdentityTests::openNgcParserPreservesMessierNgcIcIdentifiers()
{
    const std::string payload = std::string(kOpenNgcHeader)
                                + "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;;PGC 2557,UGC 454;"
                                  "Andromeda Galaxy\n"
                                + "NGC0001;G;00:00:00.00;+00:00:00.0;;0001;;;\n"
                                + "IC04715;Neb;05:20:00.00;-05:30:00.0;;;4715;;\n";

    const auto result =
        skygate::ephemeris::CatalogLoader::load(skygate::ephemeris::CatalogSourceType::OpenNgcCsv, payload);
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto bodies = result.catalog->bodies();
    const auto* messier = skygate::ephemeris::tests::findBodyById(bodies, "messier_031");
    QVERIFY(messier != nullptr);
    QCOMPARE(QString::fromStdString(messier->identity.sourceRecordId), QStringLiteral("NGC0224"));
    QVERIFY(hasIdentifier(messier->identity, "messier", "031"));
    QVERIFY(hasIdentifier(messier->identity, "ngc", "224"));
    QVERIFY(hasAlias(messier->identity.aliases, "M 31"));
    QVERIFY(hasAlias(messier->identity.aliases, "NGC 224"));
    QVERIFY(hasAlias(messier->identity.aliases, "Andromeda Galaxy"));

    const auto* ngc = skygate::ephemeris::tests::findBodyById(bodies, "ngc_1");
    QVERIFY(ngc != nullptr);
    QCOMPARE(QString::fromStdString(ngc->identity.sourceRecordId), QStringLiteral("NGC0001"));
    QVERIFY(hasIdentifier(ngc->identity, "ngc", "1"));
    QVERIFY(!hasIdentifier(ngc->identity, "messier", "001"));

    const auto* ic = skygate::ephemeris::tests::findBodyById(bodies, "ic_4715");
    QVERIFY(ic != nullptr);
    QCOMPARE(QString::fromStdString(ic->identity.sourceRecordId), QStringLiteral("IC04715"));
    QVERIFY(hasIdentifier(ic->identity, "ic", "4715"));
}

void CatalogObjectIdentityTests::bundledCatalogPreservesPlanetAndMessierIdentity()
{
    const auto result =
        skygate::ephemeris::CatalogLoader::load(skygate::ephemeris::CatalogSourceType::Bundled, std::string{});
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto bodies = result.catalog->bodies();
    const auto* mercury = skygate::ephemeris::tests::findBodyById(bodies, "mercury");
    QVERIFY(mercury != nullptr);
    QCOMPARE(mercury->kind, BaseCelestialBody::Kind::Planet);
    QCOMPARE(QString::fromStdString(mercury->identity.sourceRecordId), QStringLiteral("mercury"));
    QVERIFY(mercury->identity.externalIdentifiers.empty());

    const auto* messierOne = skygate::ephemeris::tests::findBodyById(bodies, "messier_001");
    QVERIFY(messierOne != nullptr);
    QCOMPARE(QString::fromStdString(messierOne->identity.sourceRecordId), QStringLiteral("messier_001"));
    QVERIFY(hasIdentifier(messierOne->identity, "messier", "001"));
    QVERIFY(hasIdentifier(messierOne->identity, "ngc", "1952"));
    QVERIFY(hasAlias(messierOne->identity.aliases, "Crab Nebula"));
    QVERIFY(hasAlias(messierOne->deepSkyObjectValue()->aliases, "Crab Nebula"));
}

void CatalogObjectIdentityTests::copyPathsPreserveIdentity()
{
    const OwnGalaxyCelestialBody star = makeStarWithIdentity();
    const DistantCelestialBody deepSkyObject = makeDeepSkyObjectWithIdentity();

    CelestialBodyCatalog catalog;
    catalog.appendBody(star);
    catalog.appendBody(deepSkyObject);

    QCOMPARE(catalog.ownGalaxyBodies().size(), std::size_t{1});
    QCOMPARE(catalog.distantBodies().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(catalog.ownGalaxyBodies()[0].identity.sourceRecordId), QStringLiteral("1"));
    QVERIFY(hasIdentifier(catalog.ownGalaxyBodies()[0].identity, "hip", "1"));
    QVERIFY(hasAlias(catalog.ownGalaxyBodies()[0].identity.aliases, "Alpha Star"));
    QCOMPARE(QString::fromStdString(catalog.distantBodies()[0].identity.sourceRecordId), QStringLiteral("NGC0224"));
    QVERIFY(hasIdentifier(catalog.distantBodies()[0].identity, "messier", "031"));
    QVERIFY(hasAlias(catalog.distantBodies()[0].identity.aliases, "Andromeda Galaxy"));

    const std::vector<const BaseCelestialBody*> bodies{&star, &deepSkyObject};
    const CelestialBodyCatalog spanCatalog(bodies);
    QCOMPARE(QString::fromStdString(spanCatalog.ownGalaxyBodies()[0].identity.sourceRecordId), QStringLiteral("1"));
    QCOMPARE(spanCatalog.ownGalaxyBodies()[0].identity.aliases, star.identity.aliases);
    QCOMPARE(spanCatalog.distantBodies()[0].identity.externalIdentifiers.size(), std::size_t{2});
}

void CatalogObjectIdentityTests::binaryRoundTripPreservesIdentity()
{
    const OwnGalaxyCelestialBody star = makeStarWithIdentity();
    const DistantCelestialBody deepSkyObject = makeDeepSkyObjectWithIdentity();

    const CelestialBodyCatalog source(
        std::vector<OwnGalaxyCelestialBody>{star},
        std::vector<DistantCelestialBody>{deepSkyObject},
        std::vector<CelestialBodyCatalog::OrderEntry>{
            {.domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 0U},
            {.domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = 0U},
        }
    );

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(source);
    QVERIFY(!payload.isEmpty());

    const std::unique_ptr<skygate::ephemeris::IStarCatalog> restored =
        skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const auto ownGalaxy = restored->catalog().ownGalaxyBodies();
    QCOMPARE(ownGalaxy.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(ownGalaxy[0].identity.sourceRecordId), QStringLiteral("1"));
    QVERIFY(hasIdentifier(ownGalaxy[0].identity, "hip", "1"));
    QVERIFY(hasIdentifier(ownGalaxy[0].identity, "hyg", "1"));
    QCOMPARE(ownGalaxy[0].identity.aliases, star.identity.aliases);

    const auto distant = restored->catalog().distantBodies();
    QCOMPARE(distant.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(distant[0].identity.sourceRecordId), QStringLiteral("NGC0224"));
    QVERIFY(hasIdentifier(distant[0].identity, "messier", "031"));
    QVERIFY(hasIdentifier(distant[0].identity, "ngc", "224"));
    QCOMPARE(distant[0].identity.aliases, deepSkyObject.identity.aliases);
    QCOMPARE(distant[0].deepSkyObject->aliases, deepSkyObject.deepSkyObject->aliases);
}

void CatalogObjectIdentityTests::rejectsLegacyBinarySchemaVersion()
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << std::uint32_t{0x53474243U} << std::uint16_t{2U};
    QVERIFY(skygate::ephemeris::CatalogBinaryCodec::deserialize(buffer) == nullptr);
}

QTEST_APPLESS_MAIN(CatalogObjectIdentityTests)

#include "CatalogObjectIdentityTests.moc"
