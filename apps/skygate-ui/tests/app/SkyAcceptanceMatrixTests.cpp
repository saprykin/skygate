#include "time/CalendarTime.hpp"
#include "CatalogCacheTestSupport.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkySettingsStore.hpp"

#include "engine/highprecision/CalcephKernelProvider.hpp"

#include "engine/highprecision/EarthOrientationProvider.hpp"
#include "EphemerisEngineFactory.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace core = skygate::core;
namespace ephemeris = skygate::ephemeris;
namespace highprecision = skygate::ephemeris::highprecision;

constexpr std::string_view kPayload = "SkyGate acceptance ephemeris update\n";
constexpr std::string_view kPayloadSha256 = "bc12fcf669402a46ba2bd1b56aa2392d93e79ef535dd79ae056e4222c0410ffd";

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

ephemeris::EphemerisDateRange
acceptanceRange(std::string id, std::string displayName, const int startYear, const int endYear)
{
    const auto start = ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        ephemeris::CivilDateTime{
            .astronomicalYear = startYear,
            .month = 1,
            .day = 1,
            .timeScale = ephemeris::TimeScale::Utc,
        }
    );
    const auto end = ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        ephemeris::CivilDateTime{
            .astronomicalYear = endYear,
            .month = 1,
            .day = 1,
            .timeScale = ephemeris::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return ephemeris::EphemerisDateRange{
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = *start,
        .end = *end,
    };
}

ephemeris::EphemerisDateRange acceptanceDE440sShortRangeRange()
{
    return acceptanceRange("acceptance-de440s-short-range", "Acceptance DE440s short range", 2000, 2050);
}

ephemeris::EphemerisDateRange acceptanceLongRange()
{
    return acceptanceRange("acceptance-long-range", "Acceptance long range", -13200, 13200);
}

ephemeris::EphemerisDataManifestAsset acceptanceKernelAsset(
    std::string id,
    std::string profileId,
    std::string version,
    std::string relativePath,
    ephemeris::EphemerisDateRange validityRange,
    const bool optional
)
{
    return ephemeris::EphemerisDataManifestAsset{
        .id = std::move(id),
        .kind = ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
        .profileId = std::move(profileId),
        .version = std::move(version),
        .sourceUrl = "https://example.invalid/ephemeris.bsp",
        .relativePath = std::move(relativePath),
        .checksum = {.algorithm = "sha256", .value = std::string{kPayloadSha256}},
        .compression =
            {
                .kind = ephemeris::EphemerisDataManifestCompressionKind::None,
                .uncompressedSizeBytes = kPayload.size(),
            },
        .validityRange = std::move(validityRange),
        .optional = optional,
    };
}

ephemeris::EphemerisDataManifest acceptanceManifest()
{
    ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "acceptance-data";
    manifest.dataSetInfo.displayName = "Acceptance data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "acceptance test";
    manifest.dataSetInfo.dateRanges.push_back(acceptanceDE440sShortRangeRange());
    manifest.dataSetInfo.dateRanges.push_back(acceptanceLongRange());
    manifest.profiles.push_back(
        ephemeris::EphemerisDataManifestProfile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        ephemeris::EphemerisDataManifestProfile{
            .id = "de441-long-range",
            .displayName = "DE441 long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
    manifest.assets.push_back(acceptanceKernelAsset(
        "de440s-kernel",
        "de440s-short-range",
        "acceptance-de440s-short-range-kernel",
        "kernels/de440s.bsp",
        acceptanceDE440sShortRangeRange(),
        false
    ));
    manifest.assets.push_back(acceptanceKernelAsset(
        "de441-kernel", "de441-long-range", "acceptance-de441-kernel", "kernels/de441.bsp", acceptanceLongRange(), true
    ));
    return manifest;
}

void writeStagedAsset(const QTemporaryDir& root, const ephemeris::EphemerisDataManifestAsset& asset)
{
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    QVERIFY(writeFile(root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), payload));
}

void writeStagedAssetsForProfile(
    const QTemporaryDir& root, const ephemeris::EphemerisDataManifest& manifest, const std::string_view profileId
)
{
    const ephemeris::EphemerisDataManifestProfile* profile = manifest.profile(profileId);
    Q_ASSERT(profile != nullptr);
    for (const std::string& assetId : profile->assetIds) {
        const ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        Q_ASSERT(asset != nullptr);
        writeStagedAsset(root, *asset);
    }
}

SkyEphemerisDataManager::StagedUpdateActivationRequest activationRequest(
    const ephemeris::EphemerisDataManifest& manifest,
    const QTemporaryDir& stagedRoot,
    const QString& writableCacheRoot,
    const QString& profileId = QStringLiteral("de440s-short-range")
)
{
    SkyEphemerisDataManager::StagedUpdateActivationRequest request;
    request.manifest = &manifest;
    request.profileId = profileId;
    request.stagedResourceRoot = stagedRoot.path();
    request.writableCacheRoot = writableCacheRoot;
    request.revisionToken = QStringLiteral("acceptance-%1-rev").arg(profileId);
    const ephemeris::EphemerisDataManifestProfile* profile = manifest.profile(profileId.toStdString());
    Q_ASSERT(profile != nullptr);
    for (const std::string& assetId : profile->assetIds) {
        const ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        Q_ASSERT(asset != nullptr);
        request.requiredKinds.push_back(asset->kind);
        auto& component = request.expectedComponents.emplace_back(asset->id, asset->kind);
        component.expectedVersion = asset->version;
        component.requiredValidityRange = asset->validityRange;
    }
    return request;
}

ephemeris::CelestialBody acceptanceMarsBody()
{
    return ephemeris::CelestialBody{
        .id = "mars",
        .displayName = "Mars",
        .type = ephemeris::CelestialBodyType::Planet,
        .ephemerisSource = ephemeris::CelestialBodyEphemerisSource::Planet,
    };
}

ephemeris::AstronomicalEpoch acceptanceEpoch(const int year)
{
    std::optional<ephemeris::AstronomicalEpoch> epoch = ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        ephemeris::CivilDateTime{
            .astronomicalYear = year,
            .month = 1,
            .day = 1,
            .timeScale = ephemeris::TimeScale::Tdb,
        }
    );
    Q_ASSERT(epoch.has_value());
    epoch->timeScale = ephemeris::TimeScale::Tdb;
    return *epoch;
}

ephemeris::EphemerisRequest acceptanceRequest(const int year)
{
    ephemeris::EphemerisRequest request;
    request.epoch = acceptanceEpoch(year);
    request.context.utcTime = core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    request.options.engineKind = ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = ephemeris::EphemerisCorrectionFlags::Geometric;
    return request;
}

ephemeris::EphemerisRequest acceptanceRequestWithoutSimpleFallback(const int year)
{
    ephemeris::EphemerisRequest request = acceptanceRequest(year);
    request.options.fallbackToSimpleEngine = false;
    return request;
}

QString displayText(ephemeris::EphemerisWarningCode code)
{
    const std::string_view text = ephemeris::ephemerisWarningText(code);
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QByteArray factoryDiagnostics(const ephemeris::EphemerisEngineFactoryResult& result)
{
    if (result.diagnostics.empty()) {
        return {};
    }

    const std::string_view text = result.diagnostics.front().displayText();
    return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

class AcceptanceCalcephKernelHandle final : public highprecision::ICalcephKernelHandle {
public:
    [[nodiscard]] highprecision::SolarSystemKernelStateResult computeGeometricStateWithVelocity(
        const ephemeris::AstronomicalEpoch&, const int targetNaifId, const int centerNaifId
    ) const override
    {
        highprecision::SolarSystemKernelStateResult result;
        if (targetNaifId == 499 && centerNaifId == 399) {
            result.positionAu = highprecision::SolarSystemKernelVector{.xAu = 1.0, .yAu = 1.0, .zAu = 0.1};
            return result;
        }
        if (targetNaifId == 399 && centerNaifId == 0) {
            result.positionAu = highprecision::SolarSystemKernelVector{.xAu = 0.5, .yAu = 0.0, .zAu = 0.0};
            result.velocityAuPerDay = highprecision::SolarSystemKernelVector{.xAu = 0.0, .yAu = 0.01, .zAu = 0.0};
            return result;
        }
        if (targetNaifId == 499 && centerNaifId == 0) {
            result.positionAu = highprecision::SolarSystemKernelVector{.xAu = 1.5, .yAu = 1.0, .zAu = 0.1};
            return result;
        }
        if (targetNaifId == 10 && centerNaifId == 399) {
            result.positionAu = highprecision::SolarSystemKernelVector{.xAu = -1.0, .yAu = 0.0, .zAu = 0.0};
            return result;
        }

        result.metadata.status = ephemeris::EphemerisResultStatus::Unsupported;
        result.metadata.addWarning(ephemeris::EphemerisWarningCode::UnsupportedBody);
        return result;
    }
};

class AcceptanceCalcephKernelRuntime final : public highprecision::ICalcephKernelRuntime {
public:
    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return true;
    }

    [[nodiscard]] highprecision::CalcephKernelOpenResult openKernel(const std::filesystem::path& path) const override
    {
        if (!QFileInfo::exists(QString::fromStdString(path.generic_string()))) {
            return {.diagnostic = "Acceptance kernel file is missing."};
        }

        return {.handle = std::make_unique<AcceptanceCalcephKernelHandle>()};
    }
};

class AcceptanceTimeScaleService final : public ephemeris::ITimeScaleService {
public:
    [[nodiscard]] ephemeris::TimeScaleConversionResult
    convert(const ephemeris::AstronomicalEpoch& epoch, const ephemeris::TimeScale targetScale) const override
    {
        ephemeris::TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = ephemeris::TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const ephemeris::CivilDateTime& dateTime, const ephemeris::TimeScale targetScale
    ) const override
    {
        ephemeris::TimeScaleConversionResult result;
        const std::optional<ephemeris::AstronomicalEpoch> epoch =
            ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        result.epoch = epoch.value_or(ephemeris::AstronomicalEpoch{});
        result.epoch.timeScale = targetScale;
        result.status = epoch.has_value() ? ephemeris::TimeScaleConversionStatus::Valid
                                          : ephemeris::TimeScaleConversionStatus::Failed;
        return result;
    }
};

class AcceptanceEarthOrientationProvider final : public ephemeris::IEarthOrientationProvider {
public:
    AcceptanceEarthOrientationProvider()
    {
        m_dataInfo.status = ephemeris::EarthOrientationDataStatus::Available;
        m_dataInfo.version = "acceptance-eop";
        m_dataInfo.provenance = "acceptance test";
    }

    [[nodiscard]] const ephemeris::EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const ephemeris::EarthOrientationTableEntry> entries() const noexcept override
    {
        return m_entries;
    }

private:
    ephemeris::EarthOrientationDataInfo m_dataInfo;
    std::vector<ephemeris::EarthOrientationTableEntry> m_entries;
};

ephemeris::EphemerisEngineFactoryResult createAcceptanceHighPrecisionEngine(
    std::shared_ptr<const ephemeris::IEphemerisDataSnapshot> activeDataSnapshot,
    const ephemeris::EphemerisDataManifest& manifest
)
{
    const std::array bodies{acceptanceMarsBody()};
    ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = ephemeris::EphemerisCorrectionFlags::Geometric;
    request.dataManifest = &manifest;
    request.activeDataSnapshot = std::move(activeDataSnapshot);
    request.timeScaleService = std::make_shared<AcceptanceTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<AcceptanceEarthOrientationProvider>();
    request.calcephKernelRuntime = std::make_shared<AcceptanceCalcephKernelRuntime>();
    request.fallbackPolicy = ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    return ephemeris::createEphemerisEngine(request);
}

void verifyDE440sShortRangeBodyState(const ephemeris::IEphemerisEngine& engine)
{
    const auto state = engine.computeBodyState(acceptanceRequest(2024), "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(ephemeris::EphemerisResultStatus::Valid)
    );
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
}

void compareCatalogCacheSnapshots(
    const SkySettingsStore::CatalogCacheSnapshot& actual, const SkySettingsStore::CatalogCacheSnapshot& expected
)
{
    QCOMPARE(actual.sourceLabel, expected.sourceLabel);
    QCOMPARE(actual.catalogPayload, expected.catalogPayload);
    QCOMPARE(actual.deepSkySourceLabel, expected.deepSkySourceLabel);
    QCOMPARE(actual.deepSkyCatalogPayload, expected.deepSkyCatalogPayload);
    QCOMPARE(actual.constellationLineRows, expected.constellationLineRows);
    QCOMPARE(actual.constellationLabelRows, expected.constellationLabelRows);
    QCOMPARE(actual.constellationLineSchemaVersion, expected.constellationLineSchemaVersion);
    QCOMPARE(actual.constellationCount, expected.constellationCount);
}

}  // namespace

class SkyAcceptanceMatrixTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void engineSelectionPersistsAcrossRestart();
    void cleanInstallOfflineDE440sShortRangeDataActivatesAndClearReturnsToBundled();
    void optionalLongRangeProfileActivationSelectsDe441Kernel();
    void ephemerisDataUpdateLeavesCatalogStateUnchanged();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyAcceptanceMatrixTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyAcceptanceMatrixTests")));
}

void SkyAcceptanceMatrixTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyAcceptanceMatrixTests::engineSelectionPersistsAcrossRestart()
{
    SkySettingsStore::StateSnapshot savedSnapshot;
    savedSnapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    savedSnapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    savedSnapshot.ephemeris.correctionPresetId = QStringLiteral("apparent-topocentric");
    savedSnapshot.ephemeris.preferredDataProfileId = QStringLiteral("de441-long-range");
    savedSnapshot.ephemerisSettingsPresent = true;

    SkySettingsStore firstStore;
    QVERIFY(firstStore.saveState(savedSnapshot));

    SkySettingsStore restartedStore;
    const auto restoredSnapshot = restartedStore.loadState();
    QVERIFY(restoredSnapshot.has_value());
    QVERIFY(restoredSnapshot->ephemerisSettingsPresent);
    QCOMPARE(
        static_cast<std::uint8_t>(restoredSnapshot->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(restoredSnapshot->ephemeris.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric)
    );
    QCOMPARE(restoredSnapshot->ephemeris.correctionPresetId, QString("apparent-topocentric"));
    QCOMPARE(restoredSnapshot->ephemeris.preferredDataProfileId, QString("de441-long-range"));
}

void SkyAcceptanceMatrixTests::cleanInstallOfflineDE440sShortRangeDataActivatesAndClearReturnsToBundled()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY(!manager.usingInstalledData());

    const ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
    QTemporaryDir bundledResourceRoot;
    QVERIFY(bundledResourceRoot.isValid());
    writeStagedAssetsForProfile(bundledResourceRoot, manifest, "de440s-short-range");
    QVERIFY(!QFileInfo::exists(bundledResourceRoot.path() + QStringLiteral("/kernels/de441.bsp")));
    manager.setBundledFallbackData(&manifest, bundledResourceRoot.path(), QStringLiteral("de440s-short-range"));

    const auto cleanInstallSnapshot = manager.activeDataSnapshot();
    QVERIFY(cleanInstallSnapshot != nullptr);
    QVERIFY(cleanInstallSnapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    QVERIFY(!cleanInstallSnapshot->solarSystemKernelAsset("de441-kernel").has_value());

    ephemeris::EphemerisEngineFactoryResult cleanInstallEngineResult =
        createAcceptanceHighPrecisionEngine(cleanInstallSnapshot, manifest);
    QVERIFY2(cleanInstallEngineResult.isSuccess(), factoryDiagnostics(cleanInstallEngineResult).constData());
    QVERIFY(cleanInstallEngineResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(cleanInstallEngineResult.engine->kind()),
        static_cast<std::uint8_t>(ephemeris::EphemerisEngineKind::HighPrecision)
    );
    verifyDE440sShortRangeBodyState(*cleanInstallEngineResult.engine);

    const auto outOfRangeState =
        cleanInstallEngineResult.engine->computeBodyState(acceptanceRequestWithoutSimpleFallback(1900), "mars");
    QVERIFY(outOfRangeState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(outOfRangeState->metadata.status),
        static_cast<std::uint8_t>(ephemeris::EphemerisResultStatus::OutOfRange)
    );
    QVERIFY(outOfRangeState->metadata.hasWarning(ephemeris::EphemerisWarningCode::DataOutOfRange));
    QVERIFY(displayText(ephemeris::EphemerisWarningCode::DataOutOfRange).contains(QStringLiteral("outside")));

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(activationRequest(manifest, bundledResourceRoot, m_settings.path()));

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: acceptance-de440s-short-range-kernel"));
    QCOMPARE(manager.longRangeKernelStatusText(), QString("Not installed"));

    const auto installedSnapshot = manager.activeDataSnapshot();
    QVERIFY(installedSnapshot != nullptr);
    QVERIFY(installedSnapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    QVERIFY(!installedSnapshot->solarSystemKernelAsset("de441-kernel").has_value());

    ephemeris::EphemerisEngineFactoryResult installedEngineResult =
        createAcceptanceHighPrecisionEngine(installedSnapshot, manifest);
    QVERIFY2(installedEngineResult.isSuccess(), factoryDiagnostics(installedEngineResult).constData());
    QVERIFY(installedEngineResult.engine != nullptr);
    verifyDE440sShortRangeBodyState(*installedEngineResult.engine);

    QVERIFY(manager.clearInstalledDataCache());
    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Bundled fallback"));
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    const auto bundledFallbackSnapshot = manager.activeDataSnapshot();
    QVERIFY(bundledFallbackSnapshot != nullptr);
    QVERIFY(bundledFallbackSnapshot->solarSystemKernelAsset("de440s-kernel").has_value());

    ephemeris::EphemerisEngineFactoryResult bundledFallbackEngineResult =
        createAcceptanceHighPrecisionEngine(bundledFallbackSnapshot, manifest);
    QVERIFY2(bundledFallbackEngineResult.isSuccess(), factoryDiagnostics(bundledFallbackEngineResult).constData());
    QVERIFY(bundledFallbackEngineResult.engine != nullptr);
    verifyDE440sShortRangeBodyState(*bundledFallbackEngineResult.engine);
}

void SkyAcceptanceMatrixTests::optionalLongRangeProfileActivationSelectsDe441Kernel()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    const ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
    QTemporaryDir bundledResourceRoot;
    QVERIFY(bundledResourceRoot.isValid());
    writeStagedAssetsForProfile(bundledResourceRoot, manifest, "de441-long-range");

    const SkyEphemerisDataManager::StagedUpdateActivationResult result = manager.activateVerifiedStagedUpdateSet(
        activationRequest(manifest, bundledResourceRoot, m_settings.path(), QStringLiteral("de441-long-range"))
    );

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: acceptance-de441-kernel"));
    QCOMPARE(manager.longRangeKernelStatusText(), QString("Installed: acceptance-de441-kernel"));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    QVERIFY(!snapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    const auto de441Kernel = snapshot->solarSystemKernelAsset("de441-kernel");
    QVERIFY(de441Kernel.has_value());
    QCOMPARE(QString::fromStdString(de441Kernel->profileId), QString("de441-long-range"));
    QCOMPARE(QString::fromStdString(de441Kernel->version), QString("acceptance-de441-kernel"));

    ephemeris::EphemerisEngineFactoryResult longRangeEngineResult =
        createAcceptanceHighPrecisionEngine(snapshot, manifest);
    QVERIFY2(longRangeEngineResult.isSuccess(), factoryDiagnostics(longRangeEngineResult).constData());
    QVERIFY(longRangeEngineResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(longRangeEngineResult.engine->kind()),
        static_cast<std::uint8_t>(ephemeris::EphemerisEngineKind::HighPrecision)
    );

    const auto state = longRangeEngineResult.engine->computeBodyState(acceptanceRequest(-1000), "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(ephemeris::EphemerisResultStatus::Valid)
    );
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QCOMPARE(state->metadata.effectiveDataValidityRange->id, std::string{"acceptance-long-range"});
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
}

void SkyAcceptanceMatrixTests::ephemerisDataUpdateLeavesCatalogStateUnchanged()
{
    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot catalogSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(store.saveCatalogCache(catalogSnapshot));

    const ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    writeStagedAssetsForProfile(stagedRoot, manifest, "de440s-short-range");

    SkyEphemerisDataManager manager(&store);
    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(activationRequest(manifest, stagedRoot, m_settings.path()));

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(manager.dataRevisionToken(), QString("acceptance-de440s-short-range-rev"));
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{1});

    const auto loadedCatalogSnapshot = store.loadCatalogCache();
    QVERIFY(loadedCatalogSnapshot.has_value());
    compareCatalogCacheSnapshots(*loadedCatalogSnapshot, catalogSnapshot);
}

QTEST_GUILESS_MAIN(SkyAcceptanceMatrixTests)

#include "SkyAcceptanceMatrixTests.moc"
