#include "EphemerisFixtureSupport.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

using namespace skygate::ephemeris::tests;

[[nodiscard]] QString smokeFixturePath()
{
    return QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/geometric_solar_system_smoke.json");
}

[[nodiscard]] QString smokeCsvFixturePath()
{
    return QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/geometric_solar_system_smoke.csv");
}

[[nodiscard]] QString writeFixture(QTemporaryDir& directory, const QByteArray& payload)
{
    const QString path = QDir(directory.path()).filePath(QStringLiteral("fixture.json"));
    QFile file(path);
    Q_ASSERT(file.open(QIODevice::WriteOnly | QIODevice::Text));
    Q_ASSERT(file.write(payload) == payload.size());
    return path;
}

}  // namespace

class EphemerisFixtureSupportTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsMetadataCompleteRaDecFixture();
    void rejectsMalformedFixture();
    void rejectsIncompleteMetadata();
    void rejectsLfsPointerPayload();
    void keepsSmokeFixtureAvailableWithoutLfs();
    void keepsCsvSmokeFixtureAvailableWithoutLfs();
    void computesAngularToleranceAcrossRaDec();
    void rejectsInvalidAngularToleranceInputs();
};

void EphemerisFixtureSupportTests::loadsMetadataCompleteRaDecFixture()
{
    QString errorText;
    const std::optional<EphemerisRaDecFixture> fixture = loadRaDecFixture(smokeFixturePath(), &errorText);

    QVERIFY2(fixture.has_value(), qPrintable(errorText));
    QCOMPARE(fixture->metadata.source, QStringLiteral("JPL Horizons"));
    QCOMPARE(fixture->metadata.sourceFrame, QStringLiteral("ICRF"));
    QCOMPARE(fixture->metadata.timeScale, QStringLiteral("TDB"));
    QCOMPARE(fixture->metadata.target, QStringLiteral("Mars barycenter/body center (499)"));
    QCOMPARE(fixture->metadata.observer, QStringLiteral("Earth geocenter (399)"));
    QVERIFY(fixture->metadata.sourceUrl.startsWith(QStringLiteral("https://")));
    QVERIFY(fixture->metadata.apiParameters.contains(QStringLiteral("COMMAND=\"499\"")));
    QCOMPARE(fixture->expected.rightAscensionHours, 17.780024903465446);
    QCOMPARE(fixture->expected.declinationDegrees, -23.953152259288466);
    QCOMPARE(fixture->toleranceDegrees, 1.0e-12);
}

void EphemerisFixtureSupportTests::rejectsMalformedFixture()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString errorText;

    const std::optional<EphemerisRaDecFixture> fixture =
        loadRaDecFixture(writeFixture(directory, QByteArrayLiteral("{not-json")), &errorText);

    QVERIFY(!fixture.has_value());
    QVERIFY(errorText.contains(QStringLiteral("JSON")));
}

void EphemerisFixtureSupportTests::rejectsIncompleteMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString errorText;

    const QByteArray payload = R"json(
{
  "metadata": {
    "source": "JPL Horizons",
    "sourceUrl": "https://ssd.jpl.nasa.gov/horizons/",
    "apiParameters": "COMMAND=\"499\"",
    "generatedDate": "2026-05-13",
    "sourceFrame": "ICRF",
    "timeScale": "TDB",
    "target": "Mars"
  },
  "expected": {
    "rightAscensionHours": 17.0,
    "declinationDegrees": -23.0
  },
  "tolerance": {
    "angularDegrees": 1.0e-9
  }
}
)json";

    const std::optional<EphemerisRaDecFixture> fixture = loadRaDecFixture(writeFixture(directory, payload), &errorText);

    QVERIFY(!fixture.has_value());
    QVERIFY(errorText.contains(QStringLiteral("observer")));
}

void EphemerisFixtureSupportTests::rejectsLfsPointerPayload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString errorText;

    const QByteArray payload = "version https://git-lfs.github.com/spec/v1\n"
                               "oid sha256:0000000000000000000000000000000000000000000000000000000000000000\n"
                               "size 42\n";
    const std::optional<EphemerisRaDecFixture> fixture = loadRaDecFixture(writeFixture(directory, payload), &errorText);

    QVERIFY(!fixture.has_value());
    QVERIFY(errorText.contains(QStringLiteral("Git LFS pointer")));
}

void EphemerisFixtureSupportTests::keepsSmokeFixtureAvailableWithoutLfs()
{
    QFile file(smokeFixturePath());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray payload = file.readAll();

    QVERIFY(!payload.isEmpty());
    QVERIFY(!isGitLfsPointerPayload(payload));
    QVERIFY(payload.contains("\"metadata\""));
}

void EphemerisFixtureSupportTests::keepsCsvSmokeFixtureAvailableWithoutLfs()
{
    QFile file(smokeCsvFixturePath());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray payload = file.readAll();

    QVERIFY(!payload.isEmpty());
    QVERIFY(!isGitLfsPointerPayload(payload));
    QVERIFY(payload.contains("JPL Horizons"));
}

void EphemerisFixtureSupportTests::computesAngularToleranceAcrossRaDec()
{
    const EphemerisRaDecExpectation expected{
        .rightAscensionHours = 23.99,
        .declinationDegrees = 1.0,
    };
    const EphemerisRaDecExpectation close{
        .rightAscensionHours = 0.0,
        .declinationDegrees = 1.0,
    };
    const EphemerisRaDecExpectation far{
        .rightAscensionHours = 1.0,
        .declinationDegrees = 1.0,
    };

    QVERIFY(angularSeparationDegrees(expected, close) < 0.2);
    QVERIFY(isWithinAngularTolerance(close, expected, 0.2));
    QVERIFY(!isWithinAngularTolerance(far, expected, 0.2));
}

void EphemerisFixtureSupportTests::rejectsInvalidAngularToleranceInputs()
{
    const EphemerisRaDecExpectation expected{
        .rightAscensionHours = 17.780024903465446,
        .declinationDegrees = -23.953152259288466,
    };
    const EphemerisRaDecExpectation nanRightAscension{
        .rightAscensionHours = std::numeric_limits<double>::quiet_NaN(),
        .declinationDegrees = expected.declinationDegrees,
    };
    const EphemerisRaDecExpectation infiniteDeclination{
        .rightAscensionHours = expected.rightAscensionHours,
        .declinationDegrees = std::numeric_limits<double>::infinity(),
    };

    QVERIFY(std::isnan(angularSeparationDegrees(nanRightAscension, expected)));
    QVERIFY(!isWithinAngularTolerance(nanRightAscension, expected, 1.0));
    QVERIFY(!isWithinAngularTolerance(infiniteDeclination, expected, 1.0));
    QVERIFY(!isWithinAngularTolerance(expected, expected, std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(!isWithinAngularTolerance(expected, expected, -1.0));
}

QTEST_APPLESS_MAIN(EphemerisFixtureSupportTests)

#include "EphemerisFixtureSupportTests.moc"
