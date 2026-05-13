#include "SkyEphemerisDataManager.hpp"

#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyContextController.hpp"
#include "SkySettingsStore.hpp"

#include <QtTest/QtTest>

#include <QFile>
#include <QSignalSpy>

namespace {

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

SkySettingsStore::EphemerisDataCacheSnapshot
installedSnapshot(const QString& kernelPath, const QString& earthOrientationPath)
{
    SkySettingsStore::EphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelPath = kernelPath;
    snapshot.installedKernelVersion = QStringLiteral("DE-test");
    snapshot.installedEarthOrientationPath = earthOrientationPath;
    snapshot.installedEarthOrientationVersion = QStringLiteral("EOP-test");
    snapshot.installedLeapSecondTableVersion = QStringLiteral("LS-test");
    snapshot.installedDeltaTDataVersion = QStringLiteral("DT-test");
    snapshot.dataRevisionToken = QStringLiteral("installed-rev");
    snapshot.lastUpdateResult = QStringLiteral("Installed");
    return snapshot;
}

}  // namespace

class SkyEphemerisDataManagerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void initialBundledStatus();
    void installedDataStatusAndSnapshot();
    void missingInstalledDataFallsBackToBundled();
    void revisionSignalEmitsOnlyWhenActiveDataChanges();
    void controllerOwnsManagerAndExposesSnapshot();
    void managerDoesNotTouchCatalogState();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyEphemerisDataManagerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisDataManagerTests")));
}

void SkyEphemerisDataManagerTests::init()
{
    m_settings.clearSettings();
}

void SkyEphemerisDataManagerTests::initialBundledStatus()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Bundled fallback"));
    QCOMPARE(manager.datasetInfoText(), QString("Bundled fallback"));
    QVERIFY(manager.activeDataSnapshot() != nullptr);
    QVERIFY(!manager.activeDataSnapshot()->earthOrientationDataAsset().has_value());
}

void SkyEphemerisDataManagerTests::installedDataStatusAndSnapshot()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("kernel.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("eop.txt"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel placeholder")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop payload")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(installedSnapshot(kernelPath, earthOrientationPath)));

    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Installed data active"));
    QCOMPARE(manager.dataRevisionToken(), QString("installed-rev"));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("Kernel DE-test")));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("EOP EOP-test")));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    const auto eopAsset = snapshot->earthOrientationDataAsset();
    QVERIFY(eopAsset.has_value());
    QCOMPARE(QString::fromStdString(eopAsset->content), QString("eop payload"));
}

void SkyEphemerisDataManagerTests::missingInstalledDataFallsBackToBundled()
{
    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        installedSnapshot(m_settings.filePath(QStringLiteral("missing-kernel.bsp")), QString())
    ));

    SkyEphemerisDataManager manager(&store);
    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    QVERIFY(manager.statusText().contains(QStringLiteral("Installed data missing")));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("Bundled fallback")));
    QVERIFY(manager.activeDataSnapshot() != nullptr);
}

void SkyEphemerisDataManagerTests::revisionSignalEmitsOnlyWhenActiveDataChanges()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();
    QSignalSpy activeDataSpy(&manager, &SkyEphemerisDataManager::activeDataChanged);
    QSignalSpy revisionSpy(&manager, &SkyEphemerisDataManager::dataRevisionChanged);

    const QString kernelPath = m_settings.filePath(QStringLiteral("revision-kernel.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(store.saveEphemerisDataCache(installedSnapshot(kernelPath, QString())));

    QVERIFY(manager.restoreFromSettings());
    QCOMPARE(activeDataSpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
    QVERIFY(manager.dataRevision() > originalRevision);

    QVERIFY(manager.restoreFromSettings());
    QCOMPARE(activeDataSpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
}

void SkyEphemerisDataManagerTests::controllerOwnsManagerAndExposesSnapshot()
{
    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    SkyContextController controller(nullptr, nullptr, options, nullptr);

    QCOMPARE(controller.ephemerisDataStatusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY(controller.ephemerisDataRevision() > 0U);
    QVERIFY(controller.activeEphemerisDataSnapshot() != nullptr);
}

void SkyEphemerisDataManagerTests::managerDoesNotTouchCatalogState()
{
    SkySettingsStore store;
    SkyCatalogManager catalogManager(&store);
    const std::uint64_t originalCatalogRevision = catalogManager.catalogRevision();

    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.clearInstalledDataCache());

    QCOMPARE(catalogManager.catalogRevision(), originalCatalogRevision);
    QCOMPARE(catalogManager.sourceLabel(), QString("Bundled"));
}

QTEST_GUILESS_MAIN(SkyEphemerisDataManagerTests)

#include "SkyEphemerisDataManagerTests.moc"
