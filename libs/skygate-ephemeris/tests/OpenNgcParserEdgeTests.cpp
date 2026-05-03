#include "skygate/ephemeris/CatalogPayloadParser.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>

namespace {

constexpr std::string_view kHeader =
    "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;Identifiers;Common names;NED notes;OpenNGC notes\n";

const skygate::ephemeris::CelestialBody* findBody(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::string& id
)
{
    const auto it = std::find_if(
        bodies.begin(),
        bodies.end(),
        [&id](const skygate::ephemeris::CelestialBody& body) {
            return body.id == id;
        }
    );
    return it == bodies.end() ? nullptr : &(*it);
}

bool hasAlias(
    const skygate::ephemeris::CelestialBody& body,
    const std::string& alias
)
{
    return body.deepSkyObject.has_value()
        && std::find(
            body.deepSkyObject->aliases.begin(),
            body.deepSkyObject->aliases.end(),
            alias
        ) != body.deepSkyObject->aliases.end();
}

}  // namespace

class OpenNgcParserEdgeTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMissingRequiredColumns();
    void skipsInvalidCoordinatesButKeepsValidRows();
    void rejectsPayloadWithNoUsableDeepSkyRows();
    void mapsSupportedObjectKinds();
    void preservesValidGeometryFields();
    void normalizesAndDeduplicatesAliases();
};

void OpenNgcParserEdgeTests::rejectsMissingRequiredColumns()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA\n"
        "NGC0001;G;00:00:00.00\n"
    );

    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadErrorCode::MissingRequiredColumns);
}

void OpenNgcParserEdgeTests::skipsInvalidCoordinatesButKeepsValidRows()
{
    const std::string payload = std::string(kHeader)
        + "BADRA;G;25:00:00.00;+00:00:00.0;And;;;;;;;;;;;;;;;;;1;0001;;;;;;\n"
        + "BADDEC;G;00:00:00.00;+91:00:00.0;And;;;;;;;;;;;;;;;;;2;0002;;;;;;\n"
        + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;31;0224;;;PGC 2557,UGC 454;Andromeda Galaxy;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);

    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);
    const auto bodies = result.catalog->bodies();
    QCOMPARE(bodies.size(), 1U);
    QVERIFY(findBody(bodies, "messier_031") != nullptr);
}

void OpenNgcParserEdgeTests::rejectsPayloadWithNoUsableDeepSkyRows()
{
    const std::string payload = std::string(kHeader)
        + "SKIP0001;Star;00:00:00.00;+00:00:00.0;Psc;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
        + "BADRA;G;99:00:00.00;+00:00:00.0;And;;;;;;;;;;;;;;;;;1;0001;;;;;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);

    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadErrorCode::InvalidOpenNgcCsv);
}

void OpenNgcParserEdgeTests::mapsSupportedObjectKinds()
{
    const std::string payload = std::string(kHeader)
        + "NGC0001;G;00:00:00.00;+00:00:00.0;And;;;;;;;;;;;;;;;;;;0001;;;;;;\n"
        + "NGC0002;OCl;01:00:00.00;+01:00:00.0;And;;;;;;;;;;;;;;;;;;0002;;;;;;\n"
        + "NGC0003;GCl;02:00:00.00;+02:00:00.0;And;;;;;;;;;;;;;;;;;;0003;;;;;;\n"
        + "NGC0004;Neb;03:00:00.00;+03:00:00.0;And;;;;;;;;;;;;;;;;;;0004;;;;;;\n"
        + "NGC0005;PN;04:00:00.00;+04:00:00.0;And;;;;;;;;;;;;;;;;;;0005;;;;;;\n"
        + "NGC0006;*Ass;05:00:00.00;+05:00:00.0;And;;;;;;;;;;;;;;;;;;0006;;;;;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);
    const auto bodies = result.catalog->bodies();

    QVERIFY(findBody(bodies, "ngc_1")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::Galaxy);
    QVERIFY(findBody(bodies, "ngc_2")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::OpenCluster);
    QVERIFY(findBody(bodies, "ngc_3")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::GlobularCluster);
    QVERIFY(findBody(bodies, "ngc_4")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::Nebula);
    QVERIFY(findBody(bodies, "ngc_5")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula);
    QVERIFY(findBody(bodies, "ngc_6")->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::Asterism);
}

void OpenNgcParserEdgeTests::preservesValidGeometryFields()
{
    const std::string payload = std::string(kHeader)
        + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;0;4.36;3.44;;;;13.35;SA(s)b;;;;31;0224;;;PGC 2557;Andromeda Galaxy;;\n"
        + "NGC0001;G;00:00:00.00;+00:00:00.0;And;-1;0;-5;;;;;;;SA(s)b;;;;;0001;;;;;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto* m31 = findBody(result.catalog->bodies(), "messier_031");
    QVERIFY(m31 != nullptr);
    QVERIFY(m31->deepSkyObject.has_value());
    QVERIFY(m31->deepSkyObject->majorAxisArcmin.has_value());
    QVERIFY(m31->deepSkyObject->minorAxisArcmin.has_value());
    QVERIFY(m31->deepSkyObject->positionAngleDeg.has_value());
    QCOMPARE(*m31->deepSkyObject->majorAxisArcmin, 177.83);
    QCOMPARE(*m31->deepSkyObject->minorAxisArcmin, 69.66);
    QCOMPARE(*m31->deepSkyObject->positionAngleDeg, 0.0);

    const auto* ngc1 = findBody(result.catalog->bodies(), "ngc_1");
    QVERIFY(ngc1 != nullptr);
    QVERIFY(ngc1->deepSkyObject.has_value());
    QVERIFY(!ngc1->deepSkyObject->majorAxisArcmin.has_value());
    QVERIFY(!ngc1->deepSkyObject->minorAxisArcmin.has_value());
    QVERIFY(!ngc1->deepSkyObject->positionAngleDeg.has_value());
}

void OpenNgcParserEdgeTests::normalizesAndDeduplicatesAliases()
{
    const std::string payload = std::string(kHeader)
        + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;31;0224;;;NGC0224,NGC 224,M 031;Andromeda Galaxy,Andromeda Galaxy;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto bodies = result.catalog->bodies();
    const auto* m31 = findBody(bodies, "messier_031");
    QVERIFY(m31 != nullptr);
    QCOMPARE(m31->displayName, std::string("M31"));
    QVERIFY(hasAlias(*m31, "M 31"));
    QVERIFY(hasAlias(*m31, "NGC 224"));
    QVERIFY(hasAlias(*m31, "Andromeda Galaxy"));

    const auto duplicateCount = static_cast<int>(std::count(
        m31->deepSkyObject->aliases.begin(),
        m31->deepSkyObject->aliases.end(),
        std::string("Andromeda Galaxy")
    ));
    QCOMPARE(duplicateCount, 1);
}

QTEST_APPLESS_MAIN(OpenNgcParserEdgeTests)

#include "OpenNgcParserEdgeTests.moc"
