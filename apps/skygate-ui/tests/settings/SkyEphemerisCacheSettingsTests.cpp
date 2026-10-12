#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QSettings>
#include <QtTest/QtTest>

class SkyEphemerisCacheSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesLoadsAndClearsEphemerisDataCacheMetadata();
    void ephemerisDataCachePathsRoundTripExactly();
    void partialAndMalformedCacheMetadataFallsBack();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyEphemerisCacheSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisCacheSettingsTests")));
}

void SkyEphemerisCacheSettingsTests::savesLoadsAndClearsEphemerisDataCacheMetadata()
{
    m_settings.resetSettingsWithCatalogCachePaths(
        QStringLiteral("ephemeris-clear-star-cache.csv"), QStringLiteral("ephemeris-clear-deep-sky-cache.csv")
    );

    SkySettingsStore store;
    const SkyCatalogLegacyCacheSnapshot catalogSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(store.saveCatalogCache(catalogSnapshot));

    SkyEphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    savedSnapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    savedSnapshot.installedKernelPath = m_settings.filePath(QStringLiteral("de440s.bsp"));
    savedSnapshot.installedKernelVersion = QStringLiteral("DE440s-2026a");
    savedSnapshot.installedEarthOrientationPath = m_settings.filePath(QStringLiteral("eop.csv"));
    savedSnapshot.installedEarthOrientationVersion = QStringLiteral("IERS-2026-05");
    savedSnapshot.installedLeapSecondTablePath = m_settings.filePath(QStringLiteral("leap-seconds.list"));
    savedSnapshot.installedLeapSecondTableVersion = QStringLiteral("leap-seconds-2025");
    savedSnapshot.installedDeltaTDataPath = m_settings.filePath(QStringLiteral("delta-t.csv"));
    savedSnapshot.installedDeltaTDataVersion = QStringLiteral("delta-t-2026");
    savedSnapshot.dataRevisionToken = QStringLiteral("ephemeris-rev-42");
    savedSnapshot.lastUpdateResult = QStringLiteral("Updated");

    QVERIFY(store.saveEphemerisDataCache(savedSnapshot));
    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(loadedSnapshot.installedKernelAssetId, savedSnapshot.installedKernelAssetId);
    QCOMPARE(loadedSnapshot.installedKernelProfileId, savedSnapshot.installedKernelProfileId);
    QCOMPARE(loadedSnapshot.installedKernelPath, savedSnapshot.installedKernelPath);
    QCOMPARE(loadedSnapshot.installedKernelVersion, savedSnapshot.installedKernelVersion);
    QCOMPARE(loadedSnapshot.installedEarthOrientationPath, savedSnapshot.installedEarthOrientationPath);
    QCOMPARE(loadedSnapshot.installedEarthOrientationVersion, savedSnapshot.installedEarthOrientationVersion);
    QCOMPARE(loadedSnapshot.installedLeapSecondTablePath, savedSnapshot.installedLeapSecondTablePath);
    QCOMPARE(loadedSnapshot.installedLeapSecondTableVersion, savedSnapshot.installedLeapSecondTableVersion);
    QCOMPARE(loadedSnapshot.installedDeltaTDataPath, savedSnapshot.installedDeltaTDataPath);
    QCOMPARE(loadedSnapshot.installedDeltaTDataVersion, savedSnapshot.installedDeltaTDataVersion);
    QCOMPARE(loadedSnapshot.dataRevisionToken, savedSnapshot.dataRevisionToken);
    QCOMPARE(loadedSnapshot.lastUpdateResult, savedSnapshot.lastUpdateResult);

    QVERIFY(store.clearEphemerisDataCache());
    const auto clearedSnapshot = store.loadEphemerisDataCache();
    QVERIFY(clearedSnapshot.installedKernelAssetId.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelProfileId.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelPath.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedEarthOrientationPath.isEmpty());
    QVERIFY(clearedSnapshot.installedEarthOrientationVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedLeapSecondTablePath.isEmpty());
    QVERIFY(clearedSnapshot.installedLeapSecondTableVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedDeltaTDataPath.isEmpty());
    QVERIFY(clearedSnapshot.installedDeltaTDataVersion.isEmpty());
    QCOMPARE(clearedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(clearedSnapshot.lastUpdateResult, QString("Bundled fallback"));

    const auto stillLoadedCatalogSnapshot = store.loadCatalogCache();
    QVERIFY(stillLoadedCatalogSnapshot.has_value());
    QCOMPARE(stillLoadedCatalogSnapshot->catalogPayload, catalogSnapshot.catalogPayload);
    QCOMPARE(stillLoadedCatalogSnapshot->deepSkyCatalogPayload, catalogSnapshot.deepSkyCatalogPayload);
}

void SkyEphemerisCacheSettingsTests::ephemerisDataCachePathsRoundTripExactly()
{
    QSettings settings;
    settings.clear();

    SkyEphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced kernel.bsp")));
    savedSnapshot.installedEarthOrientationPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced eop.csv")));
    savedSnapshot.installedLeapSecondTablePath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced leap-seconds.list")));
    savedSnapshot.installedDeltaTDataPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced delta-t.csv")));

    const SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(savedSnapshot));

    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(loadedSnapshot.installedKernelPath, savedSnapshot.installedKernelPath);
    QCOMPARE(loadedSnapshot.installedEarthOrientationPath, savedSnapshot.installedEarthOrientationPath);
    QCOMPARE(loadedSnapshot.installedLeapSecondTablePath, savedSnapshot.installedLeapSecondTablePath);
    QCOMPARE(loadedSnapshot.installedDeltaTDataPath, savedSnapshot.installedDeltaTDataPath);
}

void SkyEphemerisCacheSettingsTests::partialAndMalformedCacheMetadataFallsBack()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/ephemerisData/installedKernelVersion", "DE440s-2026a");
    settings.setValue("skyContext/ephemerisData/dataRevisionToken", "   ");
    settings.setValue("skyContext/ephemerisData/lastUpdateResult", "");

    const SkySettingsStore store;
    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QVERIFY(loadedSnapshot.installedKernelPath.isEmpty());
    QCOMPARE(loadedSnapshot.installedKernelVersion, QString("DE440s-2026a"));
    QVERIFY(loadedSnapshot.installedEarthOrientationPath.isEmpty());
    QVERIFY(loadedSnapshot.installedEarthOrientationVersion.isEmpty());
    QVERIFY(loadedSnapshot.installedLeapSecondTablePath.isEmpty());
    QVERIFY(loadedSnapshot.installedLeapSecondTableVersion.isEmpty());
    QVERIFY(loadedSnapshot.installedDeltaTDataPath.isEmpty());
    QVERIFY(loadedSnapshot.installedDeltaTDataVersion.isEmpty());
    QCOMPARE(loadedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(loadedSnapshot.lastUpdateResult, QString("Bundled fallback"));
}

QTEST_GUILESS_MAIN(SkyEphemerisCacheSettingsTests)

#include "SkyEphemerisCacheSettingsTests.moc"
