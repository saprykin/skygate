#include "CelestialBodyCatalog.hpp"
#include "EphemerisDataCacheSnapshotFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyContextController.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkySettingsStore.hpp"
#include "StagedEphemerisAssetFixture.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QString>
#include <QUrl>
#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using skygate::ui::tests::EphemerisDataCacheSnapshotFixture;
using skygate::ui::tests::SettingsTestFixture;
using skygate::ui::tests::StagedEphemerisAssetFixture;

namespace {

constexpr std::string_view kEmptySha256 = StagedEphemerisAssetFixture::kEmptyPayloadSha256;

QString singleAssetManifestJson(const QString& sourceUrl, const QString& version)
{
    return QStringLiteral(R"({
  "schemaVersion": 1,
  "id": "test-data",
  "displayName": "Test data",
  "version": "2026a",
  "provenance": "controller test",
  "dateRanges": [
    {
      "id": "test-range",
      "displayName": "Test range",
      "start": "1900-01-01",
      "end": "2100-01-01"
    }
  ],
  "profiles": [
    {
      "id": "de440s-short-range",
      "displayName": "DE440sShortRange",
      "bundled": false,
      "longRange": false,
      "assetIds": ["de440s-kernel"]
    }
  ],
  "assets": [
    {
      "id": "de440s-kernel",
      "kind": "solar-system-kernel",
      "profileId": "de440s-short-range",
      "version": "%1",
      "sourceUrl": "%2",
      "relativePath": "kernels/de440s.bsp",
      "checksum": {
        "algorithm": "sha256",
        "value": "%3"
      },
      "compression": {
        "format": "none",
        "uncompressedSizeBytes": 0
      },
      "validityRange": {
        "id": "kernel-range",
        "displayName": "Kernel range",
        "start": "1900-01-01",
        "end": "2100-01-01"
      }
    }
  ]
})")
        .arg(version, sourceUrl, QString::fromLatin1(kEmptySha256.data(), static_cast<qsizetype>(kEmptySha256.size())));
}

skygate::ephemeris::EphemerisDataManifest parseSingleAssetManifest(const QString& sourceUrl, const QString& version)
{
    const QString payload = singleAssetManifestJson(sourceUrl, version);
    const QByteArray bytes = payload.toUtf8();
    skygate::ephemeris::EphemerisDataManifest::ParseResult result = skygate::ephemeris::EphemerisDataManifest::parse(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))
    );
    if (!result.isSuccess()) {
        const QString diagnostic =
            result.diagnostics.empty() ? QStringLiteral("unknown") : QString::fromStdString(result.diagnostics.front());
        QTest::qFail(qPrintable(QStringLiteral("Test manifest did not parse: %1").arg(diagnostic)), __FILE__, __LINE__);
        return {};
    }
    return std::move(result.manifest);
}

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

skygate::ephemeris::EphemerisEngineOptions highPrecisionOptions()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    options.setEnableAtmosphericRefraction(false);
    options.setAtmosphericPressureHpa(875.0);
    options.setAtmosphericTemperatureC(-4.0);
    options.setRelativeHumidity(0.4);
    options.setObservingWavelengthMicrometers(0.7);
    return options;
}

void verifyHighPrecisionOptions(const skygate::ephemeris::IEphemerisEngine& engine)
{
    QCOMPARE(
        static_cast<std::uint8_t>(engine.kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );

    const skygate::ephemeris::EphemerisEngineOptions options = engine.options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::geometric())
    );
    QCOMPARE(options.enableAtmosphericRefraction(), false);
    QCOMPARE(options.atmosphericPressureHpa(), 875.0);
    QCOMPARE(options.atmosphericTemperatureC(), -4.0);
    QCOMPARE(options.relativeHumidity(), 0.4);
    QCOMPARE(options.observingWavelengthMicrometers(), 0.7);
}

class ConfiguredEphemerisEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit ConfiguredEphemerisEngine(skygate::ephemeris::EphemerisEngineOptions options) : m_options(options) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return m_options.engineKind();
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Configured ephemeris test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override
    {
        return m_options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
                   ? skygate::ephemeris::EphemerisEngineTraits::highPrecisionEngine()
                   : skygate::ephemeris::EphemerisEngineTraits::noTraits();
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        return computeBodyState(request.context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::size_t bodyIndex) const override
    {
        return computeBodyState(request.context, bodyIndex);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>();
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        return std::nullopt;
    }

private:
    skygate::ephemeris::EphemerisEngineOptions m_options;
};

}  // namespace

class SkyContextControllerEphemerisDataTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void controllerOwnsManagerAndExposesSnapshot();
    void controllerUpdateRefreshesRemoteManifestAndReportsProgress();
    void controllerCatalogChangePreservesEphemerisDataSelection();
    void controllerCatalogChangePreservesSelectedEngineConfiguration();
    void controllerActiveDataChangePreservesSelectedEngineConfiguration();
    void managerDoesNotTouchCatalogState();

private:
    SettingsTestFixture m_settings;
};

void SkyContextControllerEphemerisDataTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerEphemerisDataTests")));
}

void SkyContextControllerEphemerisDataTests::init()
{
    m_settings.clearSettings();
}

void SkyContextControllerEphemerisDataTests::controllerOwnsManagerAndExposesSnapshot()
{
    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    SkyContextController controller(nullptr, nullptr, options, nullptr);

    QCOMPARE(controller.ephemerisDataStatusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY(controller.ephemerisDataRevision() > 0U);
    QVERIFY(controller.activeEphemerisDataSnapshot() != nullptr);
}

void SkyContextControllerEphemerisDataTests::controllerUpdateRefreshesRemoteManifestAndReportsProgress()
{
    const QString initialKernelPath = m_settings.filePath(QStringLiteral("initial-remote-manifest-kernel.bsp"));
    const QString freshKernelPath = m_settings.filePath(QStringLiteral("fresh-remote-manifest-kernel.bsp"));
    const QString remoteManifestPath = m_settings.filePath(QStringLiteral("remote-manifest.json"));
    QVERIFY(writeFile(initialKernelPath, QByteArray{}));
    QVERIFY(writeFile(freshKernelPath, QByteArray{}));

    const QString freshKernelUrl = QUrl::fromLocalFile(freshKernelPath).toString();
    QVERIFY(writeFile(remoteManifestPath, singleAssetManifestJson(freshKernelUrl, QStringLiteral("fresh")).toUtf8()));

    SkySettingsStore::StateSnapshot savedSettings;
    savedSettings.ephemerisSettingsPresent = true;
    savedSettings.ephemeris.updateManifestUrl = QUrl::fromLocalFile(remoteManifestPath).toString();
    SkySettingsStore settingsStore;
    QVERIFY(settingsStore.saveState(savedSettings));

    skygate::ephemeris::EphemerisDataManifest initialManifest =
        parseSingleAssetManifest(QUrl::fromLocalFile(initialKernelPath).toString(), QStringLiteral("initial"));

    SkyContextController::InitializationOptions options;
    options.loadSettings = true;
    options.initializeLocation = false;
    options.ephemerisFactoryInputs.dataManifest = &initialManifest;
    options.ephemerisFactoryInputs.updateResourceRoot = m_settings.path();
    options.ephemerisFactoryInputs.writableCacheRoot = m_settings.path() + QStringLiteral("/remote-manifest-cache");
    SkyContextController controller(nullptr, nullptr, options, nullptr);

    QSignalSpy statusSpy(&controller, &SkyContextController::ephemerisDataStatusTextChanged);
    QVERIFY(controller.ephemerisDataUpdateEnabled());
    QVERIFY(controller.updateEphemerisDataProfile(QStringLiteral("de440s-short-range")));
    QVERIFY(statusSpy.count() > 0);
    QCOMPARE(controller.ephemerisDataUpdateProgress(), 1.0);
    QCOMPARE(controller.ephemerisShortRangeKernelStatusText(), QString("Installed: fresh"));
    QCOMPARE(controller.ephemerisDataLastUpdateResultText(), QString("Installed DE440sShortRange"));

    const auto snapshot = controller.activeEphemerisDataSnapshot();
    QVERIFY(snapshot != nullptr);
    const auto kernel = snapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(kernel.has_value());
    QCOMPARE(
        QString::fromStdString(kernel->activePath).endsWith(QStringLiteral("/de440s-short-range/kernels/de440s.bsp")),
        true
    );
}

void SkyContextControllerEphemerisDataTests::controllerCatalogChangePreservesEphemerisDataSelection()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("controller-catalog-kernel.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("controller-catalog-eop.txt"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, earthOrientationPath)
    ));

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

void SkyContextControllerEphemerisDataTests::controllerCatalogChangePreservesSelectedEngineConfiguration()
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

void SkyContextControllerEphemerisDataTests::controllerActiveDataChangePreservesSelectedEngineConfiguration()
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
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, earthOrientationPath)
    ));

    static_cast<void>(controller.loadSettings());

    QVERIFY(controller.ephemerisDataRevision() > originalDataRevision);
    QVERIFY(controller.ephemerisEngine() != nullptr);
    verifyHighPrecisionOptions(*controller.ephemerisEngine());
}

void SkyContextControllerEphemerisDataTests::managerDoesNotTouchCatalogState()
{
    SkySettingsStore store;
    SkyCatalogManager catalogManager(&store);
    const std::uint64_t originalCatalogRevision = catalogManager.catalogRevision();

    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.clearInstalledDataCache());

    QCOMPARE(catalogManager.catalogRevision(), originalCatalogRevision);
    QCOMPARE(catalogManager.sourceLabel(), QString("Bundled"));
}

QTEST_GUILESS_MAIN(SkyContextControllerEphemerisDataTests)

#include "SkyContextControllerEphemerisDataTests.moc"
