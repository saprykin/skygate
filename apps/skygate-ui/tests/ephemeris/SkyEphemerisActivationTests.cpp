#include "engine/EphemerisDataManifest.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/EphemerisDataCacheSnapshotFixture.hpp"
#include "support/SettingsTestFixture.hpp"
#include "support/StagedEphemerisAssetFixture.hpp"
#include "support/EphemerisDataManagerTestSupport.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using skygate::ui::tests::usesInstalledData;

using skygate::ui::tests::EphemerisDataCacheSnapshotFixture;
using skygate::ui::tests::SettingsTestFixture;
using skygate::ui::tests::StagedEphemerisAssetFixture;

namespace {

constexpr std::string_view kPayload = StagedEphemerisAssetFixture::kPayload;
constexpr std::string_view kEmptySha256 = StagedEphemerisAssetFixture::kEmptyPayloadSha256;

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

skygate::ephemeris::EphemerisDataManifest stagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = false,
            .longRange = false,
            .assetIds = {"de440s-kernel", "leap-seconds", "earth-orientation", "delta-t"},
        }
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "de440s-kernel",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel,
            "kernels/de440s.bsp"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "leap-seconds",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
            "time/leap-seconds.list"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "earth-orientation",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
            "time/eop.csv"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "delta-t", skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData, "time/delta-t.csv"
        )
    );
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest longRangeStagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de441-long-range",
            .displayName = "DE441 long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel", "de441-leap-seconds", "de441-earth-orientation", "de441-delta-t"},
        }
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "de441-kernel",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel,
            "kernels/de441.bsp",
            "de441-long-range"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "de441-leap-seconds",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
            "time/leap-seconds.list",
            "de441-long-range"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "de441-earth-orientation",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
            "time/eop.csv",
            "de441-long-range"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "de441-delta-t",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData,
            "time/delta-t.csv",
            "de441-long-range"
        )
    );
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest supportDataStagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "support-data",
            .displayName = "Time and Earth data",
            .bundled = false,
            .longRange = false,
            .assetIds = {"support-leap-seconds", "support-earth-orientation", "support-delta-t"},
        }
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "support-leap-seconds",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
            "time/leap-seconds.list",
            "support-data"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "support-earth-orientation",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
            "time/eop.csv",
            "support-data"
        )
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "support-delta-t",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData,
            "time/delta-t.csv",
            "support-data"
        )
    );
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest emptySingleAssetStagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = false,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );

    skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "de440s-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    asset.checksum.value = std::string{kEmptySha256};
    asset.compression.uncompressedSizeBytes = 0U;
    manifest.assets.push_back(std::move(asset));
    return manifest;
}

// A short-range profile whose id, asset id, and version name no kernel model,
// so only the recorded range can decide the displayed kernel text.
skygate::ephemeris::EphemerisDataManifest customMetadataStagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "custom-profile",
            .displayName = "Custom profile",
            .bundled = false,
            .longRange = false,
            .assetIds = {"custom-kernel"},
        }
    );
    manifest.assets.push_back(
        StagedEphemerisAssetFixture::stagedAsset(
            "custom-kernel",
            skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel,
            "kernels/custom.bsp",
            "custom-profile"
        )
    );
    return manifest;
}

void writeStagedAssets(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        QVERIFY(writeFile(root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), payload));
    }
}

SkyEphemerisDataManager::StagedUpdateActivationRequest stagedActivationRequest(
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
    request.revisionToken = QStringLiteral("installed-rev-2");
    const skygate::ephemeris::EphemerisDataManifest::Profile* profile = manifest.profile(profileId.toStdString());
    Q_ASSERT(profile != nullptr);
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
        Q_ASSERT(asset != nullptr);
        request.requiredKinds.push_back(asset->kind);
        auto& component = request.expectedComponents.emplace_back(asset->id, asset->kind);
        component.expectedVersion = "test";
        component.requiredValidityRange = StagedEphemerisAssetFixture::validityRange();
    }
    return request;
}


}  // namespace

class SkyEphemerisActivationTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void activatesVerifiedStagedUpdateSetAtomically();
    void activatesFullLongRangeProfileUpdateSet();
    void activatedCustomProfileRendersDe440s();
    void supportDataActivationPreservesInstalledKernel();
    void activatesLiveSupportDataUpdateWithDriftedPayload();
    void clearPlanetaryKernelCachePreservesSupportData();
    void clearSupportDataCachePreservesInstalledKernel();
    void activationFailurePreservesActiveDataAndSettings();
    void sameRevisionFailureKeepsActiveFilesAndSettings();
    void metadataPersistenceFailurePreservesActiveData();
    void cancelBeforeVerificationKeepsDataAndStaging();
    void cancelDuringVerificationCleansStagingKeepsData();
    void cancelBeforeActivationKeepsStagingAndActiveData();
    void cancelDuringActivationCleansCacheKeepsActiveData();

private:
    SettingsTestFixture m_settings;
};

void SkyEphemerisActivationTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisActivationTests")));
}

void SkyEphemerisActivationTests::init()
{
    m_settings.clearSettings();
}

void SkyEphemerisActivationTests::activatesVerifiedStagedUpdateSetAtomically()
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
    QVERIFY(usesInstalledData(manager));
    QCOMPARE(manager.activeCacheSnapshot().dataRevisionToken, QString("installed-rev-2"));
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{4});

    const SkyEphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(savedSnapshot.dataRevisionToken, QString("installed-rev-2"));
    QCOMPARE(savedSnapshot.installedKernelAssetId, QString("de440s-kernel"));
    QCOMPARE(savedSnapshot.installedKernelProfileId, QString("de440s-short-range"));
    QCOMPARE(savedSnapshot.installedKernelVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedEarthOrientationVersion, QString("test"));
    QCOMPARE(
        savedSnapshot.installedLeapSecondTablePath.contains(
            QStringLiteral("/updates/installed-rev-2/de440s-short-range/time/")
        ),
        true
    );
    QCOMPARE(savedSnapshot.installedLeapSecondTableVersion, QString("test"));
    QCOMPARE(
        savedSnapshot.installedDeltaTDataPath.contains(
            QStringLiteral("/updates/installed-rev-2/de440s-short-range/time/")
        ),
        true
    );
    QCOMPARE(savedSnapshot.installedDeltaTDataVersion, QString("test"));
    const QString expectedKernelPathFragment = QStringLiteral("/updates/installed-rev-2/de440s-short-range/kernels/");
    const QString expectedEopPathFragment = QStringLiteral("/updates/installed-rev-2/de440s-short-range/time/");
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

void SkyEphemerisActivationTests::activatesFullLongRangeProfileUpdateSet()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = longRangeStagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const SkyEphemerisDataManager::StagedUpdateActivationResult result = manager.activateVerifiedStagedUpdateSet(
        stagedActivationRequest(manifest, stagedRoot, m_settings.path(), QStringLiteral("de441-long-range"))
    );

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{4});
    QVERIFY(usesInstalledData(manager));
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: test"));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    QVERIFY(!snapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    const auto kernel = snapshot->solarSystemKernelAsset("de441-kernel");
    QVERIFY(kernel.has_value());
    QCOMPARE(QString::fromStdString(kernel->profileId), QString("de441-long-range"));
    QVERIFY(snapshot->leapSecondTableAsset().has_value());
    QVERIFY(snapshot->earthOrientationDataAsset().has_value());
    QVERIFY(snapshot->deltaTDataAsset().has_value());
}

void SkyEphemerisActivationTests::activatedCustomProfileRendersDe440s()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = customMetadataStagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const SkyEphemerisDataManager::StagedUpdateActivationResult result = manager.activateVerifiedStagedUpdateSet(
        stagedActivationRequest(manifest, stagedRoot, m_settings.path(), QStringLiteral("custom-profile"))
    );

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QVERIFY(usesInstalledData(manager));
    QVERIFY(manager.activeCacheSnapshot().installedKernelLongRange.has_value());
    QCOMPARE(manager.activeCacheSnapshot().installedKernelLongRange.value(), false);
    QCOMPARE(manager.kernelDisplayText(), QString("DE440s"));
}

void SkyEphemerisActivationTests::supportDataActivationPreservesInstalledKernel()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.restoreFromSettings());

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = supportDataStagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const SkyEphemerisDataManager::StagedUpdateActivationResult result = manager.activateVerifiedStagedUpdateSet(
        stagedActivationRequest(manifest, stagedRoot, m_settings.path(), QStringLiteral("support-data"))
    );

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{3});

    const SkyEphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(savedSnapshot.installedKernelAssetId, oldSnapshot.installedKernelAssetId);
    QCOMPARE(savedSnapshot.installedKernelProfileId, oldSnapshot.installedKernelProfileId);
    QCOMPARE(savedSnapshot.installedKernelPath, oldKernelPath);
    QCOMPARE(savedSnapshot.installedKernelVersion, oldSnapshot.installedKernelVersion);
    QCOMPARE(savedSnapshot.installedEarthOrientationVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedLeapSecondTableVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedDeltaTDataVersion, QString("test"));
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: DE-test"));
    QCOMPARE(manager.earthOrientationStatusText(), QString("Installed: test"));
}

void SkyEphemerisActivationTests::activatesLiveSupportDataUpdateWithDriftedPayload()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = supportDataStagedManifest();
    for (skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        asset.live = true;
        asset.checksum.algorithm.clear();
        asset.checksum.value = std::string(64U, '0');
        asset.compression.uncompressedSizeBytes = 999U;
    }

    const QByteArray driftedPayload("Drifted live support data payload that does not match pinned metadata.\n");
    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        QVERIFY(writeFile(
            stagedRoot.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), driftedPayload
        ));
    }

    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const SkyEphemerisDataManager::StagedUpdateActivationResult result = manager.activateVerifiedStagedUpdateSet(
        stagedActivationRequest(manifest, stagedRoot, m_settings.path(), QStringLiteral("support-data"))
    );

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{3});
    QVERIFY(usesInstalledData(manager));

    const SkyEphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(savedSnapshot.installedEarthOrientationVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedLeapSecondTableVersion, QString("test"));
    QCOMPARE(savedSnapshot.installedDeltaTDataVersion, QString("test"));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    const auto leapSeconds = snapshot->leapSecondTableAsset();
    QVERIFY(leapSeconds.has_value());
    QCOMPARE(QString::fromStdString(leapSeconds->content), QString::fromUtf8(driftedPayload));
    const auto eop = snapshot->earthOrientationDataAsset();
    QVERIFY(eop.has_value());
    QCOMPARE(QString::fromStdString(eop->content), QString::fromUtf8(driftedPayload));
    const auto deltaT = snapshot->deltaTDataAsset();
    QVERIFY(deltaT.has_value());
    QCOMPARE(QString::fromStdString(deltaT->content), QString::fromUtf8(driftedPayload));
}

void SkyEphemerisActivationTests::clearPlanetaryKernelCachePreservesSupportData()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("kernel-cache/de440s.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("support/eop.csv"));
    const QString leapSecondPath = m_settings.filePath(QStringLiteral("support/leap-seconds.list"));
    const QString deltaTPath = m_settings.filePath(QStringLiteral("support/delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArray(2 * 1024 * 1024, 'k')));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop")));
    QVERIFY(writeFile(leapSecondPath, QByteArrayLiteral("leap")));
    QVERIFY(writeFile(deltaTPath, QByteArrayLiteral("delta")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(
            kernelPath, earthOrientationPath, leapSecondPath, deltaTPath
        )
    ));
    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.restoreFromSettings());
    QCOMPARE(manager.planetaryKernelCacheSizeBytes(), std::uint64_t{2U * 1024U * 1024U});

    QVERIFY(manager.clearPlanetaryKernelCache());

    const SkyEphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QVERIFY(savedSnapshot.installedKernelAssetId.isEmpty());
    QVERIFY(savedSnapshot.installedKernelProfileId.isEmpty());
    QVERIFY(savedSnapshot.installedKernelPath.isEmpty());
    QVERIFY(savedSnapshot.installedKernelVersion.isEmpty());
    QCOMPARE(savedSnapshot.installedEarthOrientationPath, earthOrientationPath);
    QCOMPARE(savedSnapshot.installedLeapSecondTablePath, leapSecondPath);
    QCOMPARE(savedSnapshot.installedDeltaTDataPath, deltaTPath);
    QVERIFY(!QFileInfo::exists(kernelPath));
    QVERIFY(QFileInfo::exists(earthOrientationPath));
    QCOMPARE(manager.planetaryKernelCacheSizeBytes(), std::uint64_t{0U});
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Bundled fallback"));
    QCOMPARE(manager.earthOrientationStatusText(), QString("Installed: EOP-test"));
}

void SkyEphemerisActivationTests::clearSupportDataCachePreservesInstalledKernel()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("kernel-cache/de440s.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("support/eop.csv"));
    const QString leapSecondPath = m_settings.filePath(QStringLiteral("support/leap-seconds.list"));
    const QString deltaTPath = m_settings.filePath(QStringLiteral("support/delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop")));
    QVERIFY(writeFile(leapSecondPath, QByteArrayLiteral("leap")));
    QVERIFY(writeFile(deltaTPath, QByteArrayLiteral("delta")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(
            kernelPath, earthOrientationPath, leapSecondPath, deltaTPath
        )
    ));
    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.restoreFromSettings());
    QVERIFY(manager.supportDataCacheSizeBytes() > 0U);

    QVERIFY(manager.clearSupportDataCache());

    const SkyEphemerisDataCacheSnapshot savedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(savedSnapshot.installedKernelPath, kernelPath);
    QCOMPARE(savedSnapshot.installedKernelVersion, QString("DE-test"));
    QVERIFY(savedSnapshot.installedEarthOrientationPath.isEmpty());
    QVERIFY(savedSnapshot.installedLeapSecondTablePath.isEmpty());
    QVERIFY(savedSnapshot.installedDeltaTDataPath.isEmpty());
    QVERIFY(QFileInfo::exists(kernelPath));
    QVERIFY(!QFileInfo::exists(earthOrientationPath));
    QVERIFY(!QFileInfo::exists(leapSecondPath));
    QVERIFY(!QFileInfo::exists(deltaTPath));
    QCOMPARE(manager.supportDataCacheSizeBytes(), std::uint64_t{0U});
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: DE-test"));
    QCOMPARE(manager.earthOrientationStatusText(), QString("Bundled fallback"));
}

void SkyEphemerisActivationTests::activationFailurePreservesActiveDataAndSettings()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
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

    // A staged payload that no longer matches its recorded checksum fails
    // verification before anything is activated, and the staged set is kept for
    // a retry just like after an activation failure.
    const QString stagedKernelPath = stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp");
    QVERIFY(writeFile(stagedKernelPath, QByteArrayLiteral("corrupt")));
    const SkyEphemerisDataManager::StagedUpdateActivationResult verificationFailure =
        manager.activateVerifiedStagedUpdateSet(stagedActivationRequest(manifest, stagedRoot, m_settings.path()));

    QCOMPARE(
        static_cast<std::uint8_t>(verificationFailure.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::VerificationFailed)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(verificationFailure.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::ChecksumMismatch)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(QFileInfo::exists(stagedKernelPath));
}

void SkyEphemerisActivationTests::sameRevisionFailureKeepsActiveFilesAndSettings()
{
    const QString activeRoot = m_settings.filePath(QStringLiteral("updates/installed-rev"));
    const QString oldKernelPath = activeRoot + QStringLiteral("/de440s-short-range/kernels/de440s.bsp");
    const QString oldLeapSecondPath = activeRoot + QStringLiteral("/de440s-short-range/time/leap-seconds.list");
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));
    QVERIFY(writeFile(oldLeapSecondPath, QByteArrayLiteral("old leap seconds")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString(), oldLeapSecondPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const QString blockingTimePath =
        m_settings.filePath(QStringLiteral("updates/installed-rev-activation/de440s-short-range/time"));
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
    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.activationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::IoError)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(QFileInfo::exists(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp")));

    QFile oldKernelFile(oldKernelPath);
    QVERIFY(oldKernelFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(oldKernelFile.readAll(), QByteArray("old kernel"));
    QFile oldLeapSecondFile(oldLeapSecondPath);
    QVERIFY(oldLeapSecondFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(oldLeapSecondFile.readAll(), QByteArray("old leap seconds"));
    QVERIFY(!QFileInfo::exists(m_settings.filePath(QStringLiteral("updates/installed-rev-activation"))));
}

void SkyEphemerisActivationTests::metadataPersistenceFailurePreservesActiveData()
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
    QVERIFY(!usesInstalledData(manager));
    QCOMPARE(manager.activeCacheSnapshot().dataRevisionToken, QString("bundled"));
    QVERIFY(!QFileInfo::exists(m_settings.filePath(QStringLiteral("updates/installed-rev-2"))));
}

void SkyEphemerisActivationTests::cancelBeforeVerificationKeepsDataAndStaging()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
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

void SkyEphemerisActivationTests::cancelDuringVerificationCleansStagingKeepsData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("verify-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
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

void SkyEphemerisActivationTests::cancelBeforeActivationKeepsStagingAndActiveData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("pre-activate-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = emptySingleAssetStagedManifest();
    QVERIFY(writeFile(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp"), QByteArray{}));

    int cancellationChecks = 0;
    SkyEphemerisDataManager::StagedUpdateActivationRequest request =
        stagedActivationRequest(manifest, stagedRoot, m_settings.path());
    request.requiredKinds.clear();
    request.expectedComponents.clear();
    request.cancellationRequested = [&cancellationChecks] {
        ++cancellationChecks;
        return cancellationChecks >= 6;
    };

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.activationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::InvalidRequest)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(QFileInfo::exists(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp")));
    QVERIFY(!QFileInfo::exists(m_settings.filePath(QStringLiteral("updates/installed-rev-2"))));
}

void SkyEphemerisActivationTests::cancelDuringActivationCleansCacheKeepsActiveData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("install-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkyEphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = stagedManifest();
    writeStagedAssets(stagedRoot, manifest);

    const QString partialActivationRoot = m_settings.filePath(QStringLiteral("updates/installed-rev-2"));
    const QString firstActivatedAsset =
        partialActivationRoot + QStringLiteral("/de440s-short-range/kernels/de440s.bsp");

    SkyEphemerisDataManager::StagedUpdateActivationRequest request =
        stagedActivationRequest(manifest, stagedRoot, m_settings.path());
    request.cancellationRequested = [&firstActivatedAsset] { return QFileInfo::exists(firstActivatedAsset); };

    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled)
    );

    QCOMPARE(
        static_cast<std::uint8_t>(result.verificationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.activationStatus),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::Activated)
    );
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{1});
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(!QFileInfo::exists(partialActivationRoot));
    QVERIFY(QFileInfo::exists(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp")));
}

QTEST_GUILESS_MAIN(SkyEphemerisActivationTests)

#include "SkyEphemerisActivationTests.moc"
