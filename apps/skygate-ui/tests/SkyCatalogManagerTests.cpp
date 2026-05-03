#include "SkyCatalogManager.hpp"
#include "SkySettingsStore.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

class SkyCatalogManagerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void unknownPresetsUpdateStatus();
    void bundledPresetResetsCatalogAndConstellationRefs();
    void bundledDeepSkyPresetRebuildsActiveCatalog();
    void clearCacheReportsStatusAndSignals();
    void restoreCachePathThroughManager();

private:
    SkySettingsStore::CatalogCacheSnapshot makeCacheSnapshot() const;

private:
    QTemporaryDir m_settingsDir;
};

void SkyCatalogManagerTests::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QCoreApplication::setOrganizationName("SkygateTests");
    QCoreApplication::setApplicationName("SkyCatalogManagerTests");
}

void SkyCatalogManagerTests::init()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/catalogCachePath", m_settingsDir.filePath("catalog-cache.txt"));
    settings.setValue(
        "skyContext/deepSkyCatalogCachePath",
        m_settingsDir.filePath("deep-sky-catalog-cache.txt")
    );
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogManagerTests::makeCacheSnapshot() const
{
    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = "Custom";
    snapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";
    snapshot.deepSkySourceLabel = "OpenNGC";
    snapshot.deepSkyCatalogPayload =
        "Name;Type;RA;Dec;M;NGC;IC\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;\n";
    snapshot.constellationLineRows = "hip_1|hip_2\n";
    snapshot.constellationLabelRows = "Demo|hip_1,hip_2\n";
    snapshot.constellationLineSchemaVersion = 4;
    snapshot.constellationCount = 1;
    return snapshot;
}

void SkyCatalogManagerTests::unknownPresetsUpdateStatus()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset("unknown");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown preset 'unknown'"));
    QCOMPARE(statusSpy.count(), 1);

    manager.loadDeepSkyCatalogPreset("unknown_dso");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown deep-sky preset 'unknown_dso'"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::bundledPresetResetsCatalogAndConstellationRefs()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.setCatalogPresetIndex(99);
    manager.loadCatalogPreset("bundled");

    QCOMPARE(manager.catalogPresetIndex(), 0);
    QCOMPARE(manager.sourceLabel(), QString("Bundled"));
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.constellationLineRefs().size() > 0U);
    QVERIFY(catalogSpy.count() >= 1);
}

void SkyCatalogManagerTests::bundledDeepSkyPresetRebuildsActiveCatalog()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy infoSpy(&manager, &SkyCatalogManager::deepSkyCatalogInfoTextChanged);

    manager.setDeepSkyCatalogPresetIndex(99);
    manager.loadDeepSkyCatalogPreset("bundled_messier");

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 0);
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.deepSkyCatalogInfoText().contains("Objects:"));
    QVERIFY(infoSpy.count() >= 1);
}

void SkyCatalogManagerTests::clearCacheReportsStatusAndSignals()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    QVERIFY(manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Star catalog cache cleared"));
    QVERIFY(manager.clearDeepSkyCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Deep-sky catalog cache cleared"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::restoreCachePathThroughManager()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(makeCacheSnapshot()));

    SkyCatalogManager manager(&store);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    manager.setCatalogPresetIndex(2);
    manager.setDeepSkyCatalogPresetIndex(2);

    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceLabel(), QString("Custom (saved)"));
    QCOMPARE(manager.constellationCount(), 1U);
    QCOMPARE(manager.constellationLineRefs().size(), 1U);
    QVERIFY(manager.bodyCount() > 0U);
    QVERIFY(catalogSpy.count() >= 1);
}

QTEST_GUILESS_MAIN(SkyCatalogManagerTests)

#include "SkyCatalogManagerTests.moc"
