#include <QtTest>

#include "SkySettingsStore.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

class SkySettingsStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesAndLoadsStateSnapshot();
    void savesLoadsAndClearsCatalogCache();

private:
    QTemporaryDir m_settingsDir;
};

void SkySettingsStoreTests::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QCoreApplication::setOrganizationName("SkygateTests");
    QCoreApplication::setApplicationName("SkygateUiSettingsStoreTests");
}

void SkySettingsStoreTests::savesAndLoadsStateSnapshot()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/utcDateLocked", true);
    settings.setValue("skyContext/utcTimeLocked", false);

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot savedSnapshot;
    savedSnapshot.live = false;
    savedSnapshot.timelineToolbarCollapsed = true;
    savedSnapshot.speedMultiplier = 4.0;
    savedSnapshot.stepSeconds = 300;
    savedSnapshot.magnitudeCutoff = 7.5;
    savedSnapshot.viewCenterAltitudeDeg = 18.0;
    savedSnapshot.viewCenterAzimuthDeg = 220.0;
    savedSnapshot.viewFieldOfViewDeg = 74.0;
    savedSnapshot.utcEpochSeconds = 1'717'276'800;
    savedSnapshot.latitudeDeg = 47.0;
    savedSnapshot.longitudeDeg = 8.0;
    savedSnapshot.elevationMeters = 409.0;
    savedSnapshot.locationSourceText = "City";
    savedSnapshot.selectedCityId = "ch-zurich";
    savedSnapshot.projectionTypeText = "Perspective";
    savedSnapshot.catalogPresetIndex = 2;
    savedSnapshot.catalogUrlText = "https://example.com/catalog.csv";

    QVERIFY(store.saveState(savedSnapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->live, savedSnapshot.live);
    QCOMPARE(loadedSnapshot->timelineToolbarCollapsed, savedSnapshot.timelineToolbarCollapsed);
    QCOMPARE(loadedSnapshot->speedMultiplier, savedSnapshot.speedMultiplier);
    QCOMPARE(loadedSnapshot->stepSeconds, savedSnapshot.stepSeconds);
    QCOMPARE(loadedSnapshot->utcEpochSeconds, savedSnapshot.utcEpochSeconds);
    QCOMPARE(loadedSnapshot->locationSourceText, savedSnapshot.locationSourceText);
    QCOMPARE(loadedSnapshot->selectedCityId, savedSnapshot.selectedCityId);
    QCOMPARE(loadedSnapshot->projectionTypeText, savedSnapshot.projectionTypeText);
    QCOMPARE(loadedSnapshot->catalogPresetIndex, savedSnapshot.catalogPresetIndex);
    QCOMPARE(loadedSnapshot->catalogUrlText, savedSnapshot.catalogUrlText);

    QSettings verifySettings;
    QVERIFY(!verifySettings.contains("skyContext/utcDateLocked"));
    QVERIFY(!verifySettings.contains("skyContext/utcTimeLocked"));
}

void SkySettingsStoreTests::savesLoadsAndClearsCatalogCache()
{
    QSettings settings;
    settings.clear();
    settings.setValue(
        "skyContext/catalogCachePath",
        m_settingsDir.filePath("catalog-cache-test.txt")
    );

    SkySettingsStore store;
    SkySettingsStore::CatalogCacheSnapshot savedSnapshot;
    savedSnapshot.sourceLabel = "Saved";
    savedSnapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";
    savedSnapshot.constellationLineRows = "a|b\n";
    savedSnapshot.constellationLabelRows = "Orion|hip1,hip2\n";
    savedSnapshot.constellationLineSchemaVersion = 4;
    savedSnapshot.constellationCount = 42;

    QVERIFY(store.saveCatalogCache(savedSnapshot));
    const auto loadedSnapshot = store.loadCatalogCache();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->sourceLabel, savedSnapshot.sourceLabel);
    QCOMPARE(loadedSnapshot->catalogPayload, savedSnapshot.catalogPayload);
    QCOMPARE(loadedSnapshot->constellationLineRows, savedSnapshot.constellationLineRows);
    QCOMPARE(
        loadedSnapshot->constellationLineSchemaVersion,
        savedSnapshot.constellationLineSchemaVersion
    );
    QCOMPARE(loadedSnapshot->constellationCount, savedSnapshot.constellationCount);

    QVERIFY(store.clearCatalogCache());
    QVERIFY(!store.loadCatalogCache().has_value());
}

QTEST_GUILESS_MAIN(SkySettingsStoreTests)

#include "SkySettingsStoreTests.moc"
