#include "catalog/SkyCatalogSourceCachePaths.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QtTest/QtTest>

using skygate::ui::internal::SkyCatalogSourceCachePaths;

namespace {

QString expectedDigest(const QString& instanceId)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest.left(16));
}

}  // namespace

class SkyCatalogSourceCachePathsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void digestStaysStableAndDistinct();
    void fileStemBuildsOnTheDigest();
    void generationStemEmbedsTheGeneration();
    void sidecarPathsJoinTheDirectory();
    void sidecarPatternMatchesGeneratedNames();
    void generationFromSidecarNameReadsTheStem();
    void directoryFallsBackToTheDefault();
    void directoryUsesConfiguredPath();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogSourceCachePathsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogSourceCachePathsTests")));
}

void SkyCatalogSourceCachePathsTests::digestStaysStableAndDistinct()
{
    const QString alphaDigest = SkyCatalogSourceCachePaths::idDigest(QStringLiteral("custom:alpha"));

    QCOMPARE(alphaDigest, expectedDigest(QStringLiteral("custom:alpha")));
    QCOMPARE(alphaDigest, SkyCatalogSourceCachePaths::idDigest(QStringLiteral("custom:alpha")));
    QCOMPARE(alphaDigest.size(), 16);
    QVERIFY(alphaDigest != SkyCatalogSourceCachePaths::idDigest(QStringLiteral("custom:beta")));
    // The digest of an empty identity stays a plain hash instead of a special
    // case; callers reject empty identities before naming a sidecar.
    QCOMPARE(SkyCatalogSourceCachePaths::idDigest(QString()), expectedDigest(QString()));
}

void SkyCatalogSourceCachePathsTests::fileStemBuildsOnTheDigest()
{
    const QString instanceId = QStringLiteral("preset:hyg_v42");
    QCOMPARE(
        SkyCatalogSourceCachePaths::fileStem(instanceId), QStringLiteral("catalog-source-") + expectedDigest(instanceId)
    );
}

void SkyCatalogSourceCachePathsTests::generationStemEmbedsTheGeneration()
{
    const QString instanceId = QStringLiteral("custom:alpha");
    QCOMPARE(
        SkyCatalogSourceCachePaths::generationFileStem(instanceId, 7U),
        QStringLiteral("catalog-source-g7-") + expectedDigest(instanceId)
    );
    QCOMPARE(
        SkyCatalogSourceCachePaths::generationFileStem(instanceId, 0U),
        QStringLiteral("catalog-source-g0-") + expectedDigest(instanceId)
    );
}

void SkyCatalogSourceCachePathsTests::sidecarPathsJoinTheDirectory()
{
    const QString directory = m_settings.currentTestFilePath(QStringLiteral("collection"));
    const QString instanceId = QStringLiteral("custom:alpha");
    const QString stem = SkyCatalogSourceCachePaths::generationFileStem(instanceId, 12U);

    QCOMPARE(
        SkyCatalogSourceCachePaths::rawSidecarPath(directory, instanceId, 12U),
        QDir(directory).filePath(stem + QStringLiteral(".txt"))
    );
    QCOMPARE(
        SkyCatalogSourceCachePaths::binarySidecarPath(directory, instanceId, 12U),
        QDir(directory).filePath(stem + QStringLiteral(".bin"))
    );
}

void SkyCatalogSourceCachePathsTests::sidecarPatternMatchesGeneratedNames()
{
    const QString directory = m_settings.currentTestFilePath(QStringLiteral("collection"));
    QVERIFY(QDir().mkpath(directory));

    const QString instanceId = QStringLiteral("custom:alpha");
    const QString rawPath = SkyCatalogSourceCachePaths::rawSidecarPath(directory, instanceId, 1U);
    const QString binaryPath = SkyCatalogSourceCachePaths::binarySidecarPath(directory, instanceId, 2U);
    const QString flatPath =
        QDir(directory).filePath(SkyCatalogSourceCachePaths::fileStem(instanceId) + QStringLiteral(".txt"));
    const QString manifestPath = QDir(directory).filePath(QStringLiteral("catalog-collection.manifest"));
    for (const QString& path : QStringList{rawPath, binaryPath, flatPath, manifestPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("x"), qint64(1));
    }

    const QStringList sidecars = QDir(directory).entryList(
        QStringList{SkyCatalogSourceCachePaths::sidecarEntryPattern()}, QDir::Files | QDir::Readable, QDir::Name
    );
    QCOMPARE(sidecars.size(), 3);
    QVERIFY(!sidecars.contains(QStringLiteral("catalog-collection.manifest")));
}

void SkyCatalogSourceCachePathsTests::generationFromSidecarNameReadsTheStem()
{
    const QString instanceId = QStringLiteral("custom:alpha");

    QCOMPARE(
        SkyCatalogSourceCachePaths::generationFromSidecarName(
            SkyCatalogSourceCachePaths::generationFileStem(instanceId, 42U) + QStringLiteral(".txt")
        )
            .value(),
        42U
    );
    QCOMPARE(
        SkyCatalogSourceCachePaths::generationFromSidecarName(
            SkyCatalogSourceCachePaths::generationFileStem(instanceId, 0U) + QStringLiteral(".bin")
        )
            .value(),
        0U
    );
    // Names that carry no numeric generation between the marker and the digest
    // are not sidecars of a generation, so cleanup must not read a number out
    // of them.
    QVERIFY(!SkyCatalogSourceCachePaths::generationFromSidecarName(
                 SkyCatalogSourceCachePaths::fileStem(instanceId) + QStringLiteral(".txt")
    )
                 .has_value());
    QVERIFY(!SkyCatalogSourceCachePaths::generationFromSidecarName(QStringLiteral("catalog-source-g12")).has_value());
    QVERIFY(
        !SkyCatalogSourceCachePaths::generationFromSidecarName(QStringLiteral("catalog-source-gabc-12.txt")).has_value()
    );
    QVERIFY(!SkyCatalogSourceCachePaths::generationFromSidecarName(QStringLiteral("catalog-collection.manifest"))
                 .has_value());
}

void SkyCatalogSourceCachePathsTests::directoryFallsBackToTheDefault()
{
    m_settings.clearSettings();
    QSettings settings;
    QCOMPARE(SkyCatalogSourceCachePaths::directory(settings), SkyCatalogSourceCachePaths::defaultDirectory());

    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), QString());
    QCOMPARE(SkyCatalogSourceCachePaths::directory(settings), SkyCatalogSourceCachePaths::defaultDirectory());
}

void SkyCatalogSourceCachePathsTests::directoryUsesConfiguredPath()
{
    const QString configuredPath = m_settings.currentTestFilePath(QStringLiteral("configured-cache"));
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), configuredPath);

    QCOMPARE(SkyCatalogSourceCachePaths::directory(settings), configuredPath);
}

QTEST_GUILESS_MAIN(SkyCatalogSourceCachePathsTests)

#include "SkyCatalogSourceCachePathsTests.moc"
