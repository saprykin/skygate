#include "CatalogCacheTestSupport.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkySettingsStore.hpp"
#include "StaticCalcephKernelProvider.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/EarthOrientationProvider.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

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

using namespace skygate::core;

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

skygate::ephemeris::EphemerisDateRange
acceptanceRange(std::string id, std::string displayName, const int startYear, const int endYear)
{
    const auto start = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = startYear,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    const auto end = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = endYear,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDateRange acceptanceDE440sShortRangeRange()
{
    return acceptanceRange("acceptance-de440s-short-range", "Acceptance DE440s short range", 2000, 2050);
}

skygate::ephemeris::EphemerisDateRange acceptanceLongRange()
{
    return acceptanceRange("acceptance-long-range", "Acceptance long range", -13200, 13200);
}

skygate::ephemeris::EphemerisDataManifestAsset acceptanceKernelAsset(
    std::string id,
    std::string profileId,
    std::string version,
    std::string relativePath,
    skygate::ephemeris::EphemerisDateRange validityRange,
    const bool optional
)
{
    return skygate::ephemeris::EphemerisDataManifestAsset{
        .id = std::move(id),
        .kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
        .profileId = std::move(profileId),
        .version = std::move(version),
        .sourceUrl = "https://example.invalid/ephemeris.bsp",
        .relativePath = std::move(relativePath),
        .checksum = {.algorithm = "sha256", .value = std::string{kPayloadSha256}},
        .compression =
            {
                .kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None,
                .uncompressedSizeBytes = kPayload.size(),
            },
        .validityRange = std::move(validityRange),
        .optional = optional,
    };
}

skygate::ephemeris::EphemerisDataManifest acceptanceManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "acceptance-data";
    manifest.dataSetInfo.displayName = "Acceptance data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "acceptance test";
    manifest.dataSetInfo.dateRanges.push_back(acceptanceDE440sShortRangeRange());
    manifest.dataSetInfo.dateRanges.push_back(acceptanceLongRange());
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
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

void writeStagedAsset(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifestAsset& asset)
{
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    QVERIFY(writeFile(root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), payload));
}

void writeStagedAssetsForProfile(
    const QTemporaryDir& root,
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const std::string_view profileId
)
{
    const skygate::ephemeris::EphemerisDataManifestProfile* profile = manifest.profile(profileId);
    Q_ASSERT(profile != nullptr);
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        Q_ASSERT(asset != nullptr);
        writeStagedAsset(root, *asset);
    }
}

SkyEphemerisDataManager::StagedUpdateActivationRequest activationRequest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
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
    const skygate::ephemeris::EphemerisDataManifestProfile* profile = manifest.profile(profileId.toStdString());
    Q_ASSERT(profile != nullptr);
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        Q_ASSERT(asset != nullptr);
        request.requiredKinds.push_back(asset->kind);
        auto& component = request.expectedComponents.emplace_back(asset->id, asset->kind);
        component.expectedVersion = asset->version;
        component.requiredValidityRange = asset->validityRange;
    }
    return request;
}

skygate::ephemeris::OwnGalaxyCelestialBody acceptanceMarsBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Planet;
    return body;
}

skygate::ephemeris::AstronomicalEpoch acceptanceEpoch(const int year)
{
    std::optional<skygate::ephemeris::AstronomicalEpoch> epoch =
        skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
            skygate::ephemeris::CivilDateTime{
                .astronomicalYear = year,
                .month = 1,
                .day = 1,
                .timeScale = skygate::ephemeris::TimeScale::Tdb,
            }
        );
    Q_ASSERT(epoch.has_value());
    epoch->timeScale = skygate::ephemeris::TimeScale::Tdb;
    return *epoch;
}

skygate::ephemeris::EphemerisRequest acceptanceRequest(const int year)
{
    skygate::ephemeris::EphemerisRequest request;
    request.epoch = acceptanceEpoch(year);
    request.context.utcTime = UtcTimePoint(std::chrono::seconds(1'704'067'200));
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    return request;
}

skygate::ephemeris::EphemerisRequest acceptanceRequestWithoutSimpleFallback(const int year)
{
    skygate::ephemeris::EphemerisRequest request = acceptanceRequest(year);
    request.options.setFallbackToSimpleEngine(false);
    return request;
}

QString displayText(skygate::ephemeris::EphemerisEngineWarning::Code code)
{
    const std::string_view text = skygate::ephemeris::EphemerisEngineWarning::text(code);
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QByteArray factoryDiagnostics(const skygate::ephemeris::EphemerisEngineFactoryResult& result)
{
    if (result.diagnostics.empty()) {
        return {};
    }

    const std::string_view text = result.diagnostics.front().displayText();
    return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

class AcceptanceCalcephKernel final : public skygate::ephemeris::highprecision::ICalcephKernel {
public:
    explicit AcceptanceCalcephKernel(Info info) : m_kernelInfo(std::move(info)) {}

    [[nodiscard]] Status status() const noexcept override
    {
        return Status::Ready;
    }

    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override
    {
        return m_diagnostics;
    }

    [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override
    {
        return m_kernelInfo;
    }

    [[nodiscard]] Status statusForEpoch(const skygate::ephemeris::AstronomicalEpoch& epoch) const noexcept override
    {
        if (!epoch.isFinite() || epoch.sortKey() < m_kernelInfo->validityRange.start.sortKey()
            || epoch.sortKey() > m_kernelInfo->validityRange.end.sortKey()) {
            return Status::OutOfRange;
        }

        return Status::Ready;
    }

    [[nodiscard]] skygate::ephemeris::highprecision::SolarSystemKernelStateResult compute(
        const skygate::ephemeris::AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId
    ) const override
    {
        skygate::ephemeris::highprecision::SolarSystemKernelStateResult result;
        const Status epochStatus = statusForEpoch(epoch);
        if (epochStatus != Status::Ready) {
            result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
            result.metadata.addWarning(skygate::ephemeris::EphemerisEngineWarning::Code::DataOutOfRange);
            return result;
        }

        if (targetNaifId == 499 && centerNaifId == 399) {
            result.positionAu = Vector3d{.x = 1.0, .y = 1.0, .z = 0.1};
        } else if (targetNaifId == 399 && centerNaifId == 0) {
            result.positionAu = Vector3d{.x = 0.5, .y = 0.0, .z = 0.0};
            result.velocityAuPerDay = Vector3d{.x = 0.0, .y = 0.01, .z = 0.0};
        } else if (targetNaifId == 499 && centerNaifId == 0) {
            result.positionAu = Vector3d{.x = 1.5, .y = 1.0, .z = 0.1};
        } else if (targetNaifId == 10 && centerNaifId == 399) {
            result.positionAu = Vector3d{.x = -1.0, .y = 0.0, .z = 0.0};
        } else {
            result.positionAu = Vector3d{.x = 0.75, .y = 0.25, .z = 0.1};
        }

        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.dataSourceProvenance = m_kernelInfo->provenance;
        result.metadata.effectiveDataValidityRange = m_kernelInfo->validityRange;
        return result;
    }

private:
    std::vector<std::string> m_diagnostics;
    std::optional<Info> m_kernelInfo;
};

skygate::ephemeris::highprecision::ICalcephKernel::Info acceptanceKernelInfo(
    const skygate::ephemeris::IEphemerisDataSnapshot& activeDataSnapshot,
    const skygate::ephemeris::EphemerisDataManifest& manifest
)
{
    std::optional<skygate::ephemeris::EphemerisKernelDataAsset> snapshotAsset =
        activeDataSnapshot.solarSystemKernelAsset("de441-kernel");
    if (!snapshotAsset.has_value()) {
        snapshotAsset = activeDataSnapshot.solarSystemKernelAsset("de440s-kernel");
    }

    Q_ASSERT(snapshotAsset.has_value());
    const skygate::ephemeris::EphemerisDataManifestAsset* manifestAsset = manifest.asset(snapshotAsset->id);
    const skygate::ephemeris::EphemerisDataManifestProfile* profile = manifest.profile(snapshotAsset->profileId);
    Q_ASSERT(manifestAsset != nullptr);
    Q_ASSERT(profile != nullptr);

    return skygate::ephemeris::highprecision::ICalcephKernel::Info{
        .id = snapshotAsset->id,
        .profileId = snapshotAsset->profileId,
        .version = snapshotAsset->version,
        .provenance = snapshotAsset->provenance,
        .validityRange = manifestAsset->validityRange,
        .optional = manifestAsset->optional,
        .longRange = profile->longRange,
    };
}

class AcceptanceTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convert(
        const skygate::ephemeris::AstronomicalEpoch& epoch, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        skygate::ephemeris::TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::ephemeris::CivilDateTime& dateTime, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        skygate::ephemeris::TimeScaleConversionResult result;
        const std::optional<skygate::ephemeris::AstronomicalEpoch> epoch =
            skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        result.epoch = epoch.value_or(skygate::ephemeris::AstronomicalEpoch{});
        result.epoch.timeScale = targetScale;
        result.status = epoch.has_value() ? skygate::ephemeris::TimeScaleConversionStatus::Valid
                                          : skygate::ephemeris::TimeScaleConversionStatus::Failed;
        return result;
    }
};

class AcceptanceEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    AcceptanceEarthOrientationProvider()
    {
        m_dataInfo.status = skygate::ephemeris::EarthOrientationDataStatus::Available;
        m_dataInfo.version = "acceptance-eop";
        m_dataInfo.provenance = "acceptance test";
    }

    [[nodiscard]] const skygate::ephemeris::EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::EarthOrientationTableEntry> entries() const noexcept override
    {
        return m_entries;
    }

private:
    skygate::ephemeris::EarthOrientationDataInfo m_dataInfo;
    std::vector<skygate::ephemeris::EarthOrientationTableEntry> m_entries;
};

skygate::ephemeris::EphemerisEngineFactoryResult createAcceptanceHighPrecisionEngine(
    std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot,
    const skygate::ephemeris::EphemerisDataManifest& manifest
)
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{acceptanceMarsBody()}
    );
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.dataManifest = &manifest;
    request.calcephKernelProvider = std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(
        std::make_shared<AcceptanceCalcephKernel>(acceptanceKernelInfo(*activeDataSnapshot, manifest))
    );
    request.activeDataSnapshot = std::move(activeDataSnapshot);
    request.timeScaleService = std::make_shared<AcceptanceTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<AcceptanceEarthOrientationProvider>();
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    return skygate::ephemeris::EphemerisEngineFactory::create(request);
}

void verifyDE440sShortRangeBodyState(const skygate::ephemeris::IEphemerisEngine& engine)
{
    const auto state = engine.computeBodyState(acceptanceRequest(2024), "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
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
    QCOMPARE(actual.constellationAnchorGroupRows, expected.constellationAnchorGroupRows);
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
    savedSnapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    savedSnapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric();
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
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(restoredSnapshot->ephemeris.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric())
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

    const skygate::ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
    QTemporaryDir bundledResourceRoot;
    QVERIFY(bundledResourceRoot.isValid());
    writeStagedAssetsForProfile(bundledResourceRoot, manifest, "de440s-short-range");
    QVERIFY(!QFileInfo::exists(bundledResourceRoot.path() + QStringLiteral("/kernels/de441.bsp")));
    manager.setBundledFallbackData(&manifest, bundledResourceRoot.path(), QStringLiteral("de440s-short-range"));

    const auto cleanInstallSnapshot = manager.activeDataSnapshot();
    QVERIFY(cleanInstallSnapshot != nullptr);
    QVERIFY(cleanInstallSnapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    QVERIFY(!cleanInstallSnapshot->solarSystemKernelAsset("de441-kernel").has_value());

    skygate::ephemeris::EphemerisEngineFactoryResult cleanInstallEngineResult =
        createAcceptanceHighPrecisionEngine(cleanInstallSnapshot, manifest);
    QVERIFY2(cleanInstallEngineResult.isSuccess(), factoryDiagnostics(cleanInstallEngineResult).constData());
    QVERIFY(cleanInstallEngineResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(cleanInstallEngineResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    verifyDE440sShortRangeBodyState(*cleanInstallEngineResult.engine);

    const auto outOfRangeState =
        cleanInstallEngineResult.engine->computeBodyState(acceptanceRequestWithoutSimpleFallback(1900), "mars");
    QVERIFY(outOfRangeState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(outOfRangeState->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(outOfRangeState->metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(displayText(skygate::ephemeris::EphemerisEngineWarning::Code::DataOutOfRange)
                .contains(QStringLiteral("outside")));

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

    skygate::ephemeris::EphemerisEngineFactoryResult installedEngineResult =
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

    skygate::ephemeris::EphemerisEngineFactoryResult bundledFallbackEngineResult =
        createAcceptanceHighPrecisionEngine(bundledFallbackSnapshot, manifest);
    QVERIFY2(bundledFallbackEngineResult.isSuccess(), factoryDiagnostics(bundledFallbackEngineResult).constData());
    QVERIFY(bundledFallbackEngineResult.engine != nullptr);
    verifyDE440sShortRangeBodyState(*bundledFallbackEngineResult.engine);
}

void SkyAcceptanceMatrixTests::optionalLongRangeProfileActivationSelectsDe441Kernel()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    const skygate::ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
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

    skygate::ephemeris::EphemerisEngineFactoryResult longRangeEngineResult =
        createAcceptanceHighPrecisionEngine(snapshot, manifest);
    QVERIFY2(longRangeEngineResult.isSuccess(), factoryDiagnostics(longRangeEngineResult).constData());
    QVERIFY(longRangeEngineResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(longRangeEngineResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );

    const auto state = longRangeEngineResult.engine->computeBodyState(acceptanceRequest(-1000), "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
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

    const skygate::ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
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
