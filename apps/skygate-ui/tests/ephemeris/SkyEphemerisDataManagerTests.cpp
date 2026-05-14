#include "SkyEphemerisDataManager.hpp"

#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyContextController.hpp"
#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <QtTest/QtTest>

#include <QFile>
#include <QSignalSpy>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

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
    snapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("modern");
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

skygate::ephemeris::EphemerisEngineOptions highPrecisionOptions()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Geometric;
    options.enableAtmosphericRefraction = false;
    options.atmosphericPressureHpa = 875.0;
    options.atmosphericTemperatureC = -4.0;
    options.relativeHumidity = 0.4;
    options.observingWavelengthMicrometers = 0.7;
    return options;
}

void verifyHighPrecisionOptions(const skygate::ephemeris::IEphemerisEngine& engine)
{
    QCOMPARE(
        static_cast<std::uint8_t>(engine.kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );

    const skygate::ephemeris::EphemerisEngineOptions options = engine.options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::Geometric)
    );
    QCOMPARE(options.enableAtmosphericRefraction, false);
    QCOMPARE(options.atmosphericPressureHpa, 875.0);
    QCOMPARE(options.atmosphericTemperatureC, -4.0);
    QCOMPARE(options.relativeHumidity, 0.4);
    QCOMPARE(options.observingWavelengthMicrometers, 0.7);
}

class ConfiguredEphemerisEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit ConfiguredEphemerisEngine(skygate::ephemeris::EphemerisEngineOptions options) : m_options(options) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return m_options.engineKind;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>();
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
    {
        return std::nullopt;
    }

private:
    skygate::ephemeris::EphemerisEngineOptions m_options;
};

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
    void controllerCatalogChangePreservesEphemerisDataSelection();
    void controllerCatalogChangePreservesSelectedEngineConfiguration();
    void controllerActiveDataChangePreservesSelectedEngineConfiguration();
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
    const auto kernelAsset = snapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(kernelAsset.has_value());
    QCOMPARE(QString::fromStdString(kernelAsset->id), QString("de440s-kernel"));
    QCOMPARE(QString::fromStdString(kernelAsset->profileId), QString("modern"));
    QCOMPARE(QString::fromStdString(kernelAsset->activePath), kernelPath);
    QVERIFY(!snapshot->solarSystemKernelAsset("de441-kernel").has_value());

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

void SkyEphemerisDataManagerTests::controllerCatalogChangePreservesEphemerisDataSelection()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("controller-catalog-kernel.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("controller-catalog-eop.txt"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(installedSnapshot(kernelPath, earthOrientationPath)));

    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    SkyContextController controller(nullptr, nullptr, options, nullptr);

    QCOMPARE(controller.ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));
    const std::uint64_t originalDataRevision = controller.ephemerisDataRevision();
    const std::uint64_t originalCatalogRevision = controller.catalogRevision();
    auto originalSnapshot = controller.activeEphemerisDataSnapshot();
    QVERIFY(originalSnapshot != nullptr);
    const auto originalKernel = originalSnapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(originalKernel.has_value());
    QCOMPARE(QString::fromStdString(originalKernel->activePath), kernelPath);

    controller.loadDeepSkyCatalogPreset(QStringLiteral("bundled_messier"));

    QVERIFY(controller.catalogRevision() > originalCatalogRevision);
    QCOMPARE(controller.ephemerisDataRevision(), originalDataRevision);
    auto currentSnapshot = controller.activeEphemerisDataSnapshot();
    QVERIFY(currentSnapshot != nullptr);
    const auto currentKernel = currentSnapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(currentKernel.has_value());
    QCOMPARE(QString::fromStdString(currentKernel->activePath), kernelPath);
    QVERIFY(controller.ephemerisEngine() != nullptr);
}

void SkyEphemerisDataManagerTests::controllerCatalogChangePreservesSelectedEngineConfiguration()
{
    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    SkyContextController controller(
        nullptr, std::make_unique<ConfiguredEphemerisEngine>(highPrecisionOptions()), options, nullptr
    );

    QVERIFY(controller.ephemerisEngine() != nullptr);
    verifyHighPrecisionOptions(*controller.ephemerisEngine());

    const std::uint64_t originalCatalogRevision = controller.catalogRevision();
    controller.loadDeepSkyCatalogPreset(QStringLiteral("bundled_messier"));

    QVERIFY(controller.catalogRevision() > originalCatalogRevision);
    QVERIFY(controller.ephemerisEngine() != nullptr);
    verifyHighPrecisionOptions(*controller.ephemerisEngine());
}

void SkyEphemerisDataManagerTests::controllerActiveDataChangePreservesSelectedEngineConfiguration()
{
    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    SkyContextController controller(
        nullptr, std::make_unique<ConfiguredEphemerisEngine>(highPrecisionOptions()), options, nullptr
    );

    QVERIFY(controller.ephemerisEngine() != nullptr);
    verifyHighPrecisionOptions(*controller.ephemerisEngine());
    const std::uint64_t originalDataRevision = controller.ephemerisDataRevision();

    const QString kernelPath = m_settings.filePath(QStringLiteral("controller-active-data-kernel.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("controller-active-data-eop.txt"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(installedSnapshot(kernelPath, earthOrientationPath)));

    static_cast<void>(controller.loadSettings());

    QVERIFY(controller.ephemerisDataRevision() > originalDataRevision);
    QVERIFY(controller.ephemerisEngine() != nullptr);
    verifyHighPrecisionOptions(*controller.ephemerisEngine());
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
