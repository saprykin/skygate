#include "SkyEphemerisDataManager.hpp"

#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyContextController.hpp"
#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kPayload = "SkyGate ephemeris staged asset\n";
constexpr std::string_view kPayloadSha256 = "782092fd09110da30d95c1b5bc87cd827174ae5346bb3ea80cd5384fd8cf0d13";

bool writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

SkySettingsStore::EphemerisDataCacheSnapshot installedSnapshot(
    const QString& kernelPath,
    const QString& earthOrientationPath,
    const QString& leapSecondTablePath = {},
    const QString& deltaTDataPath = {}
)
{
    SkySettingsStore::EphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("modern");
    snapshot.installedKernelPath = kernelPath;
    snapshot.installedKernelVersion = QStringLiteral("DE-test");
    snapshot.installedEarthOrientationPath = earthOrientationPath;
    snapshot.installedEarthOrientationVersion = QStringLiteral("EOP-test");
    snapshot.installedLeapSecondTablePath = leapSecondTablePath;
    snapshot.installedLeapSecondTableVersion = QStringLiteral("LS-test");
    snapshot.installedDeltaTDataPath = deltaTDataPath;
    snapshot.installedDeltaTDataVersion = QStringLiteral("DT-test");
    snapshot.dataRevisionToken = QStringLiteral("installed-rev");
    snapshot.lastUpdateResult = QStringLiteral("Installed");
    return snapshot;
}

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = 2000,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    const auto end = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = 2100,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "test-range",
        .displayName = "Test range",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifestAsset
stagedAsset(std::string id, const skygate::ephemeris::EphemerisDataManifestAssetKind kind, std::string relativePath)
{
    skygate::ephemeris::EphemerisDataManifestAsset asset;
    asset.id = std::move(id);
    asset.kind = kind;
    asset.profileId = "modern";
    asset.version = "test";
    asset.relativePath = std::move(relativePath);
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kPayloadSha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None;
    asset.compression.uncompressedSizeBytes = kPayload.size();
    asset.validityRange = testValidityRange();
    return asset;
}

skygate::ephemeris::EphemerisDataManifest stagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "modern",
        .displayName = "Modern",
        .bundled = false,
        .longRange = false,
        .assetIds = {"de440s-kernel", "leap-seconds", "earth-orientation", "delta-t"},
    });
    manifest.assets.push_back(stagedAsset(
        "de440s-kernel", skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    ));
    manifest.assets.push_back(stagedAsset(
        "leap-seconds", skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable, "time/leap-seconds.list"
    ));
    manifest.assets.push_back(stagedAsset(
        "earth-orientation", skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData, "time/eop.csv"
    ));
    manifest.assets.push_back(
        stagedAsset("delta-t", skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData, "time/delta-t.csv")
    );
    return manifest;
}

void writeStagedAssets(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    for (const skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest.assets) {
        QVERIFY(writeFile(root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), payload));
    }
}

SkyEphemerisDataManager::StagedUpdateActivationRequest stagedActivationRequest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QTemporaryDir& stagedRoot,
    const QString& writableCacheRoot
)
{
    SkyEphemerisDataManager::StagedUpdateActivationRequest request;
    request.manifest = &manifest;
    request.profileId = QStringLiteral("modern");
    request.stagedResourceRoot = stagedRoot.path();
    request.writableCacheRoot = writableCacheRoot;
    request.revisionToken = QStringLiteral("installed-rev-2");
    request.requiredKinds = {
        skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
        skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable,
        skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData,
        skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData,
    };
    request.expectedComponents = {
        {"de440s-kernel", skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel},
        {"leap-seconds", skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable},
        {"earth-orientation", skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData},
        {"delta-t", skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData},
    };
    for (skygate::ephemeris::EphemerisStagedUpdateVerificationRequest::ExpectedComponent& component :
         request.expectedComponents) {
        component.expectedVersion = "test";
        component.requiredValidityRange = testValidityRange();
    }
    return request;
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
    void activatesVerifiedStagedUpdateSetAtomically();
    void activationFailurePreservesActiveDataAndSettings();
    void sameRevisionActivationFailurePreservesActiveFilesAndSettings();
    void metadataPersistenceFailurePreservesActiveData();
    void cancellationBeforeVerificationPreservesActiveDataAndRetainsStaging();
    void cancellationDuringVerificationCanCleanStagingAndPreservesActiveData();
    void cancellationDuringActivationCleansPartialCacheAndPreservesActiveData();
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
    const QString leapSecondTablePath = m_settings.filePath(QStringLiteral("leap-seconds.list"));
    const QString deltaTDataPath = m_settings.filePath(QStringLiteral("delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel placeholder")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop payload")));
    QVERIFY(writeFile(leapSecondTablePath, QByteArrayLiteral("leap payload")));
    QVERIFY(writeFile(deltaTDataPath, QByteArrayLiteral("delta t payload")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        installedSnapshot(kernelPath, earthOrientationPath, leapSecondTablePath, deltaTDataPath)
    ));

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
    const auto leapSecondAsset = snapshot->leapSecondTableAsset();
    QVERIFY(leapSecondAsset.has_value());
    QCOMPARE(QString::fromStdString(leapSecondAsset->content), QString("leap payload"));
    const auto deltaTAsset = snapshot->deltaTDataAsset();
    QVERIFY(deltaTAsset.has_value());
    QCOMPARE(QString::fromStdString(deltaTAsset->content), QString("delta t payload"));
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

void SkyEphemerisDataManagerTests::activatesVerifiedStagedUpdateSetAtomically()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();
    QSignalSpy activeDataSpy(&manager, &SkyEphemerisDataManager::activeDataChanged);
    QSignalSpy revisionSpy(&manager, &SkyEphemerisDataManager::dataRevisionChanged);

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(stagedActivationRequest(manifest, stagedRoot, m_settings.path()));

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Activated)
    );
    QCOMPARE(activeDataSpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
    QVERIFY(manager.dataRevision() > originalRevision);
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("installed-rev-2"));
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{4});

    const SkySettingsStore::EphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(savedSnapshot.dataRevisionToken, QString("installed-rev-2"));
    QCOMPARE(savedSnapshot.installedKernelAssetId, QString("de440s-kernel"));
    QCOMPARE(savedSnapshot.installedKernelProfileId, QString("modern"));
    QCOMPARE(savedSnapshot.installedKernelVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedEarthOrientationVersion, QString("test"));
    QCOMPARE(
        savedSnapshot.installedLeapSecondTablePath.contains(QStringLiteral("/updates/installed-rev-2/modern/time/")),
        true
    );
    QCOMPARE(savedSnapshot.installedLeapSecondTableVersion, QString("test"));
    QCOMPARE(
        savedSnapshot.installedDeltaTDataPath.contains(QStringLiteral("/updates/installed-rev-2/modern/time/")), true
    );
    QCOMPARE(savedSnapshot.installedDeltaTDataVersion, QString("test"));
    const QString expectedKernelPathFragment = QStringLiteral("/updates/installed-rev-2/modern/kernels/");
    const QString expectedEopPathFragment = QStringLiteral("/updates/installed-rev-2/modern/time/");
    QVERIFY(savedSnapshot.installedKernelPath.contains(expectedKernelPathFragment));
    QVERIFY(savedSnapshot.installedEarthOrientationPath.contains(expectedEopPathFragment));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    const auto kernel = snapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(kernel.has_value());
    QCOMPARE(QString::fromStdString(kernel->activePath), savedSnapshot.installedKernelPath);
    const auto eop = snapshot->earthOrientationDataAsset();
    QVERIFY(eop.has_value());
    QCOMPARE(
        QString::fromStdString(eop->content),
        QString::fromUtf8(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );
    const auto leapSeconds = snapshot->leapSecondTableAsset();
    QVERIFY(leapSeconds.has_value());
    QCOMPARE(
        QString::fromStdString(leapSeconds->content),
        QString::fromUtf8(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );
    const auto deltaT = snapshot->deltaTDataAsset();
    QVERIFY(deltaT.has_value());
    QCOMPARE(
        QString::fromStdString(deltaT->content),
        QString::fromUtf8(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );
}

void SkyEphemerisDataManagerTests::activationFailurePreservesActiveDataAndSettings()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot = installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const QString blockingPath = m_settings.filePath(QStringLiteral("cache-root-file"));
    QVERIFY(writeFile(blockingPath, QByteArrayLiteral("not a directory")));
    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(stagedActivationRequest(manifest, stagedRoot, blockingPath));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::ActivationFailed)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
}

void SkyEphemerisDataManagerTests::sameRevisionActivationFailurePreservesActiveFilesAndSettings()
{
    const QString activeRoot = m_settings.filePath(QStringLiteral("updates/installed-rev"));
    const QString oldKernelPath = activeRoot + QStringLiteral("/modern/kernels/de440s.bsp");
    const QString oldLeapSecondPath = activeRoot + QStringLiteral("/modern/time/leap-seconds.list");
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));
    QVERIFY(writeFile(oldLeapSecondPath, QByteArrayLiteral("old leap seconds")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot =
        installedSnapshot(oldKernelPath, QString(), oldLeapSecondPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const QString blockingTimePath =
        m_settings.filePath(QStringLiteral("updates/installed-rev-activation/modern/time"));
    QVERIFY(writeFile(blockingTimePath, QByteArrayLiteral("not a directory")));

    SkyEphemerisDataManager::StagedUpdateActivationRequest request =
        stagedActivationRequest(manifest, stagedRoot, m_settings.path());
    request.revisionToken = QStringLiteral("installed-rev");
    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(request);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::ActivationFailed)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);

    QFile oldKernelFile(oldKernelPath);
    QVERIFY(oldKernelFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(oldKernelFile.readAll(), QByteArray("old kernel"));
    QFile oldLeapSecondFile(oldLeapSecondPath);
    QVERIFY(oldLeapSecondFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(oldLeapSecondFile.readAll(), QByteArray("old leap seconds"));
    QVERIFY(!QFileInfo::exists(m_settings.filePath(QStringLiteral("updates/installed-rev-activation"))));
}

void SkyEphemerisDataManagerTests::metadataPersistenceFailurePreservesActiveData()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    SkyEphemerisDataManager manager(nullptr);
    const std::uint64_t originalRevision = manager.dataRevision();

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(stagedActivationRequest(manifest, stagedRoot, m_settings.path()));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::PersistenceFailed)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    QVERIFY(!QFileInfo::exists(m_settings.filePath(QStringLiteral("updates/installed-rev-2"))));
}

void SkyEphemerisDataManagerTests::cancellationBeforeVerificationPreservesActiveDataAndRetainsStaging()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot = installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    manager.requestUpdateCancellation();
    QVERIFY(manager.updateCancellationRequested());
    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(stagedActivationRequest(manifest, stagedRoot, m_settings.path()));

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Canceled)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(QFileInfo::exists(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp")));

    manager.clearUpdateCancellation();
    QVERIFY(!manager.updateCancellationRequested());
}

void SkyEphemerisDataManagerTests::cancellationDuringVerificationCanCleanStagingAndPreservesActiveData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("verify-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot = installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const QString stagedPath = stagedRoot.path();
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    int cancellationChecks = 0;
    SkyEphemerisDataManager::StagedUpdateActivationRequest request =
        stagedActivationRequest(manifest, stagedRoot, m_settings.path());
    request.retainStagedResourcesOnCancellation = false;
    request.cancellationRequested = [&cancellationChecks] {
        ++cancellationChecks;
        return cancellationChecks >= 3;
    };

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(!QFileInfo::exists(stagedPath));
}

void SkyEphemerisDataManagerTests::cancellationDuringActivationCleansPartialCacheAndPreservesActiveData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("install-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot = installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const QString partialActivationRoot = m_settings.filePath(QStringLiteral("updates/installed-rev-2"));
    const QString firstActivatedAsset = partialActivationRoot + QStringLiteral("/modern/kernels/de440s.bsp");

    SkyEphemerisDataManager::StagedUpdateActivationRequest request =
        stagedActivationRequest(manifest, stagedRoot, m_settings.path());
    request.cancellationRequested = [&firstActivatedAsset] { return QFileInfo::exists(firstActivatedAsset); };

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(!QFileInfo::exists(partialActivationRoot));
    QVERIFY(QFileInfo::exists(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp")));
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
