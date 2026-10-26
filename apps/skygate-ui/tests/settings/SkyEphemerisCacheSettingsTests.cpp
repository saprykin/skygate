#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QSettings>
#include <QtTest/QtTest>

class SkyEphemerisCacheSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void clearingEphemerisCacheKeepsCatalogCache();
    void ephemerisDataCachePathsRoundTripExactly();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyEphemerisCacheSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisCacheSettingsTests")));
}

void SkyEphemerisCacheSettingsTests::clearingEphemerisCacheKeepsCatalogCache()
{
    m_settings.resetSettingsWithCatalogCachePaths(
        QStringLiteral("ephemeris-clear-star-cache.csv"), QStringLiteral("ephemeris-clear-deep-sky-cache.csv")
    );

    SkySettingsStore store;
    const SkyCatalogLegacyCacheSnapshot catalogSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(catalogSnapshot));

    SkyEphemerisDataCacheSnapshot ephemerisSnapshot;
    ephemerisSnapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    QVERIFY(store.saveEphemerisDataCache(ephemerisSnapshot));
    QVERIFY(store.clearEphemerisDataCache());

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

QTEST_GUILESS_MAIN(SkyEphemerisCacheSettingsTests)

#include "SkyEphemerisCacheSettingsTests.moc"
