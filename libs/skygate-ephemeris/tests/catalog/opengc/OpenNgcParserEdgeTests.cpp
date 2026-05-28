#include "catalog/CatalogLoader.hpp"
#include "catalog/CatalogPayloadParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>

namespace {

constexpr std::string_view kHeader =
    "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;SurfBr;Hubble;Cstar U-Mag;Cstar "
    "B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;Identifiers;Common names;NED notes;OpenNGC notes\n";

const skygate::ephemeris::BaseCelestialBody*
findBody(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const std::string& id)
{
    const auto it =
        std::find_if(bodies.begin(), bodies.end(), [&id](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->id == id;
        });
    return it == bodies.end() ? nullptr : *it;
}

bool hasAlias(const skygate::ephemeris::BaseCelestialBody& body, const std::string& alias)
{
    return body.deepSkyObjectValue().has_value()
           && std::find(body.deepSkyObjectValue()->aliases.begin(), body.deepSkyObjectValue()->aliases.end(), alias)
                  != body.deepSkyObjectValue()->aliases.end();
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
    QTest::ignoreMessage(
        QtWarningMsg,
        "OpenNGC CSV parse failed: OpenNGC CSV payload is missing one of the required columns: Name, Type, RA, Dec."
    );
    const auto result = skygate::ephemeris::CatalogLoader::load(
        skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
        "Name;Type;RA\n"
        "NGC0001;G;00:00:00.00\n"
    );

    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadResult::ErrorCode::MissingRequiredColumns);
}

void OpenNgcParserEdgeTests::skipsInvalidCoordinatesButKeepsValidRows()
{
    const std::string payload = std::string(kHeader)
                                + "BADRA;G;25:00:00.00;+00:00:00.0;And;;;;;;;;;;;;;;;;;1;0001;;;;;;\n"
                                + "BADDEC;G;00:00:00.00;+91:00:00.0;And;;;;;;;;;;;;;;;;;2;0002;;;;;;\n"
                                + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;"
                                  "31;0224;;;PGC 2557,UGC 454;Andromeda Galaxy;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg,
        "OpenNGC CSV skipped 2 rows with invalid coordinates; samples: row 2 name='BADRA' ra='25:00:00.00' "
        "dec='+00:00:00.0'; row 3 name='BADDEC' ra='00:00:00.00' dec='+91:00:00.0'"
    );
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
    QTest::ignoreMessage(
        QtWarningMsg,
        "OpenNGC CSV skipped 1 rows with invalid coordinates; samples: row 3 name='BADRA' ra='99:00:00.00' "
        "dec='+00:00:00.0'"
    );
    QTest::ignoreMessage(
        QtWarningMsg, "OpenNGC CSV parse failed: OpenNGC CSV payload does not contain any valid deep-sky object rows."
    );
    const auto result = parser.parseResult(payload);

    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadResult::ErrorCode::InvalidOpenNgcCsv);
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

    QVERIFY(
        findBody(bodies, "ngc_1")->deepSkyObjectValue()->kind == skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy
    );
    QVERIFY(
        findBody(bodies, "ngc_2")->deepSkyObjectValue()->kind
        == skygate::ephemeris::DeepSkyObjectInfo::Kind::OpenCluster
    );
    QVERIFY(
        findBody(bodies, "ngc_3")->deepSkyObjectValue()->kind
        == skygate::ephemeris::DeepSkyObjectInfo::Kind::GlobularCluster
    );
    QVERIFY(
        findBody(bodies, "ngc_4")->deepSkyObjectValue()->kind == skygate::ephemeris::DeepSkyObjectInfo::Kind::Nebula
    );
    QVERIFY(
        findBody(bodies, "ngc_5")->deepSkyObjectValue()->kind
        == skygate::ephemeris::DeepSkyObjectInfo::Kind::PlanetaryNebula
    );
    QVERIFY(
        findBody(bodies, "ngc_6")->deepSkyObjectValue()->kind == skygate::ephemeris::DeepSkyObjectInfo::Kind::Asterism
    );
}

void OpenNgcParserEdgeTests::preservesValidGeometryFields()
{
    const std::string payload = std::string(kHeader)
                                + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;0;4.36;3.44;;;;13.35;SA(s)b;;;;"
                                  "31;0224;;;PGC 2557;Andromeda Galaxy;;\n"
                                + "NGC0001;G;00:00:00.00;+00:00:00.0;And;-1;0;-5;;;;;;;SA(s)b;;;;;0001;;;;;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);
    QVERIFY(result.isSuccess());
    QVERIFY(result.catalog != nullptr);

    const auto* m31 = findBody(result.catalog->bodies(), "messier_031");
    QVERIFY(m31 != nullptr);
    QVERIFY(m31->deepSkyObjectValue().has_value());
    QVERIFY(m31->deepSkyObjectValue()->majorAxisArcmin.has_value());
    QVERIFY(m31->deepSkyObjectValue()->minorAxisArcmin.has_value());
    QVERIFY(m31->deepSkyObjectValue()->positionAngleDeg.has_value());
    QCOMPARE(*m31->deepSkyObjectValue()->majorAxisArcmin, 177.83);
    QCOMPARE(*m31->deepSkyObjectValue()->minorAxisArcmin, 69.66);
    QCOMPARE(*m31->deepSkyObjectValue()->positionAngleDeg, 0.0);

    const auto* ngc1 = findBody(result.catalog->bodies(), "ngc_1");
    QVERIFY(ngc1 != nullptr);
    QVERIFY(ngc1->deepSkyObjectValue().has_value());
    QVERIFY(!ngc1->deepSkyObjectValue()->majorAxisArcmin.has_value());
    QVERIFY(!ngc1->deepSkyObjectValue()->minorAxisArcmin.has_value());
    QVERIFY(!ngc1->deepSkyObjectValue()->positionAngleDeg.has_value());
}

void OpenNgcParserEdgeTests::normalizesAndDeduplicatesAliases()
{
    const std::string payload = std::string(kHeader)
                                + "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;"
                                  "31;0224;;;NGC0224,NGC 224,M 031;Andromeda Galaxy,Andromeda Galaxy;;\n";

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
        m31->deepSkyObjectValue()->aliases.begin(),
        m31->deepSkyObjectValue()->aliases.end(),
        std::string("Andromeda Galaxy")
    ));
    QCOMPARE(duplicateCount, 1);
}

QTEST_APPLESS_MAIN(OpenNgcParserEdgeTests)

#include "OpenNgcParserEdgeTests.moc"
