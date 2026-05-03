#include "skygate/ephemeris/CatalogPayloadParser.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QtTest/QtTest>

#include <QFile>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

Q_DECLARE_METATYPE(skygate::ephemeris::CatalogPayloadFormat)

#ifndef SKYGATE_EPHEMERIS_TESTDATA_DIR
#define SKYGATE_EPHEMERIS_TESTDATA_DIR ""
#endif

class CatalogFixtureCorpusTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesFixtureCorpus_data();
    void parsesFixtureCorpus();
    void parsesStellariumFixture();
    void toleratesMalformedStellariumFixture();
};

namespace {

QByteArray readFixture(const QString& relativePath)
{
    QFile file(
        QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR)
        + QStringLiteral("/")
        + relativePath
    );
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "Could not open fixture" << file.fileName();
        return {};
    }
    return file.readAll();
}

std::string_view payloadView(const QByteArray& payload)
{
    return std::string_view(
        payload.constData(),
        static_cast<std::size_t>(payload.size())
    );
}

bool containsBody(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::string& id
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&id](const skygate::ephemeris::CelestialBody& body) {
            return body.id == id;
        }
    );
}

}  // namespace

void CatalogFixtureCorpusTests::parsesFixtureCorpus_data()
{
    QTest::addColumn<QString>("fixturePath");
    QTest::addColumn<skygate::ephemeris::CatalogPayloadFormat>("expectedFormat");
    QTest::addColumn<QString>("expectedBodyId");
    QTest::addColumn<int>("expectedBodyCount");

    QTest::newRow("hyg-csv")
        << QStringLiteral("catalogs/realistic-hyg.csv")
        << skygate::ephemeris::CatalogPayloadFormat::HygCsv
        << QStringLiteral("hip_32349")
        << 3;
    QTest::newRow("open-ngc-csv")
        << QStringLiteral("catalogs/realistic-openngc.csv")
        << skygate::ephemeris::CatalogPayloadFormat::OpenNgcCsv
        << QStringLiteral("messier_031")
        << 3;
    QTest::newRow("hyg-gzip")
        << QStringLiteral("catalogs/realistic-hyg.csv.gz")
        << skygate::ephemeris::CatalogPayloadFormat::HygCsvGzip
        << QStringLiteral("hip_32349")
        << 3;
    QTest::newRow("hyg-zip")
        << QStringLiteral("catalogs/realistic-hyg.zip")
        << skygate::ephemeris::CatalogPayloadFormat::HygCsvZip
        << QStringLiteral("hip_32349")
        << 3;
    QTest::newRow("hyg-extra-columns")
        << QStringLiteral("catalogs/realistic-hyg-extra-columns.csv")
        << skygate::ephemeris::CatalogPayloadFormat::HygCsv
        << QStringLiteral("hip_7588")
        << 2;
    QTest::newRow("open-ngc-mixed")
        << QStringLiteral("catalogs/realistic-openngc-mixed.csv")
        << skygate::ephemeris::CatalogPayloadFormat::OpenNgcCsv
        << QStringLiteral("messier_042")
        << 4;
}

void CatalogFixtureCorpusTests::parsesFixtureCorpus()
{
    QFETCH(QString, fixturePath);
    QFETCH(skygate::ephemeris::CatalogPayloadFormat, expectedFormat);
    QFETCH(QString, expectedBodyId);
    QFETCH(int, expectedBodyCount);

    const QByteArray payload = readFixture(fixturePath);
    QVERIFY(!payload.isEmpty());

    const skygate::ephemeris::CatalogPayloadParser parser;
    QCOMPARE(parser.detectFormat(payloadView(payload)), expectedFormat);

    const auto parseResult = parser.parseResult(payloadView(payload));
    QVERIFY2(parseResult.isSuccess(), parseResult.errorDetail.c_str());
    QVERIFY(parseResult.catalog != nullptr);

    const auto bodies = parseResult.catalog->bodies();
    QCOMPARE(static_cast<int>(bodies.size()), expectedBodyCount);
    QVERIFY(containsBody(bodies, expectedBodyId.toStdString()));
}

void CatalogFixtureCorpusTests::parsesStellariumFixture()
{
    const QByteArray payload = readFixture(QStringLiteral("catalogs/stellarium-western.json"));
    QVERIFY(!payload.isEmpty());

    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(payloadView(payload));

    QCOMPARE(result.constellationCount, 2U);
    QVERIFY(result.lineRefs.size() >= 3U);
    QCOMPARE(result.labelRefs.size(), 2U);
    QCOMPARE(result.labelRefs[0].first, std::string("Orion"));
    QCOMPARE(result.labelRefs[1].first, std::string("Ursa Major"));
}

void CatalogFixtureCorpusTests::toleratesMalformedStellariumFixture()
{
    const QByteArray payload =
        readFixture(QStringLiteral("catalogs/stellarium-malformed-mixed.json"));
    QVERIFY(!payload.isEmpty());

    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(payloadView(payload));

    QCOMPARE(result.constellationCount, 2U);
    QCOMPARE(result.lineRefs.size(), 1U);
    QCOMPARE(result.labelRefs.size(), 1U);
    QCOMPARE(result.labelRefs.front().first, std::string("Cassiopeia"));
}

QTEST_APPLESS_MAIN(CatalogFixtureCorpusTests)

#include "CatalogFixtureCorpusTests.moc"
