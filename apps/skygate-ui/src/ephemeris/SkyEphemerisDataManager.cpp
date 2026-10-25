#include "SkyEphemerisDataManager.hpp"
#include "SkyEphemerisAssetStatusView.hpp"
#include "SkyEphemerisCacheController.hpp"
#include "SkyEphemerisDataCacheSnapshotPolicy.hpp"
#include "SkyEphemerisDownloadService.hpp"
#include "SkyEphemerisSnapshotAdapter.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"
#include "settings/SkySettingsStore.hpp"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStringList>

#include <memory>
#include <utility>

namespace {

using EphemerisDataCacheSnapshot = SkyEphemerisDataCacheSnapshot;
using StagedUpdateActivationRequest = SkyEphemerisDataManager::StagedUpdateActivationRequest;
using StagedUpdateDownloadRequest = SkyEphemerisDataManager::StagedUpdateDownloadRequest;
using skygate::ui::skygateEphemerisDataLog;
using skygate::ui::internal::SkyEphemerisAssetStatusView;
using skygate::ui::internal::SkyEphemerisDataCacheSnapshotPolicy;

bool hasInstalledAssetMetadata(const EphemerisDataCacheSnapshot& snapshot)
{
    return !snapshot.installedKernelAssetId.isEmpty() || !snapshot.installedKernelProfileId.isEmpty()
           || !snapshot.installedKernelPath.isEmpty() || !snapshot.installedKernelVersion.isEmpty()
           || !snapshot.installedEarthOrientationPath.isEmpty() || !snapshot.installedEarthOrientationVersion.isEmpty()
           || !snapshot.installedLeapSecondTablePath.isEmpty() || !snapshot.installedLeapSecondTableVersion.isEmpty()
           || !snapshot.installedDeltaTDataPath.isEmpty() || !snapshot.installedDeltaTDataVersion.isEmpty();
}

bool hasInstalledMetadata(const EphemerisDataCacheSnapshot& snapshot)
{
    return hasInstalledAssetMetadata(snapshot)
           || snapshot.dataRevisionToken != EphemerisDataCacheSnapshot{}.dataRevisionToken;
}

QStringList missingInstalledPaths(const EphemerisDataCacheSnapshot& snapshot)
{
    QStringList missingPaths;
    for (const QString& path : EphemerisDataCacheSnapshot::installedPayloadPaths(snapshot)) {
        if (!path.isEmpty() && !QFileInfo::exists(path)) {
            missingPaths.push_back(path);
        }
    }
    return missingPaths;
}

// Removes the installed payload files of one cache and reports the first file
// that could not be removed. Empty paths are skipped: a cache never installed
// that asset owns no file.
bool removeInstalledFiles(const QStringList& paths, const QString& cacheLabel)
{
    for (const QString& path : paths) {
        if (path.isEmpty()) {
            continue;
        }
        QFile file(path);
        if (file.exists() && !file.remove()) {
            qCWarning(skygateEphemerisDataLog).noquote()
                << "Unable to remove" << cacheLabel << "cache file" << path << file.errorString();
            return false;
        }
    }
    return true;
}

bool cacheSnapshotsEqual(const EphemerisDataCacheSnapshot& lhs, const EphemerisDataCacheSnapshot& rhs)
{
    return lhs.installedKernelAssetId == rhs.installedKernelAssetId
           && lhs.installedKernelProfileId == rhs.installedKernelProfileId
           && lhs.installedKernelPath == rhs.installedKernelPath
           && lhs.installedKernelVersion == rhs.installedKernelVersion
           && lhs.installedKernelLongRange == rhs.installedKernelLongRange
           && lhs.installedEarthOrientationPath == rhs.installedEarthOrientationPath
           && lhs.installedEarthOrientationVersion == rhs.installedEarthOrientationVersion
           && lhs.installedLeapSecondTablePath == rhs.installedLeapSecondTablePath
           && lhs.installedLeapSecondTableVersion == rhs.installedLeapSecondTableVersion
           && lhs.installedDeltaTDataPath == rhs.installedDeltaTDataPath
           && lhs.installedDeltaTDataVersion == rhs.installedDeltaTDataVersion
           && lhs.dataRevisionToken == rhs.dataRevisionToken && lhs.lastUpdateResult == rhs.lastUpdateResult;
}

// Both staged requests carry the same cancellation callback, so one helper
// combines them with the manager's own flag instead of keeping two copies.
template <typename Request>
Request combineCancellation(Request request, const SkyEphemerisDataManager* manager)
{
    const auto original = request.cancellationRequested;
    request.cancellationRequested = [manager, original] {
        return manager->updateCancellationRequested() || (original != nullptr && original());
    };
    return request;
}

}  // namespace

SkyEphemerisDataManager::SkyEphemerisDataManager(
    SkySettingsStore* settingsStore, QObject* parent, QNetworkAccessManager* networkAccessManager
)
    : QObject(parent), m_settingsStore(settingsStore),
      m_downloadService(std::make_unique<SkyEphemerisDownloadService>(networkAccessManager, this))
{
    static_cast<void>(restoreFromSettings());
}

SkyEphemerisDataManager::~SkyEphemerisDataManager() = default;

QString SkyEphemerisDataManager::statusText() const
{
    return m_assetStatusView.statusText();
}

QString SkyEphemerisDataManager::shortRangeKernelStatusText() const
{
    return m_assetStatusView.shortRangeKernelStatusText();
}

QString SkyEphemerisDataManager::kernelDisplayText() const
{
    return m_assetStatusView.kernelDisplayText();
}

QString SkyEphemerisDataManager::earthOrientationStatusText() const
{
    return m_assetStatusView.earthOrientationStatusText();
}

QString SkyEphemerisDataManager::leapSecondStatusText() const
{
    return m_assetStatusView.leapSecondStatusText();
}

QString SkyEphemerisDataManager::deltaTStatusText() const
{
    return m_assetStatusView.deltaTStatusText();
}

std::uint64_t SkyEphemerisDataManager::planetaryKernelCacheSizeBytes() const
{
    return m_assetStatusView.planetaryKernelCacheSizeBytes();
}

std::uint64_t SkyEphemerisDataManager::supportDataCacheSizeBytes() const
{
    return m_assetStatusView.supportDataCacheSizeBytes();
}

QString SkyEphemerisDataManager::lastUpdateResultText() const
{
    return m_assetStatusView.lastUpdateResultText();
}

std::uint64_t SkyEphemerisDataManager::dataRevision() const noexcept
{
    return m_dataRevision;
}

SkyEphemerisDataCacheSnapshot SkyEphemerisDataManager::activeCacheSnapshot() const
{
    return m_activeCacheSnapshot;
}

std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>
SkyEphemerisDataManager::activeDataSnapshot() const noexcept
{
    return m_activeDataSnapshot;
}

void SkyEphemerisDataManager::setBundledFallbackData(
    const skygate::ephemeris::EphemerisDataManifest* manifest, QString resourceRoot, QString profileId
)
{
    m_bundledFallbackManifest = manifest;
    m_bundledFallbackResourceRoot = std::move(resourceRoot).trimmed();
    m_bundledFallbackProfileId = std::move(profileId).trimmed();

    if (m_activeSource != ActiveSource::Installed) {
        m_activeDataSnapshot.reset();
        applyCacheSnapshot(m_activeCacheSnapshot, m_activeSource, true);
    }
}

bool SkyEphemerisDataManager::restoreFromSettings()
{
    const EphemerisDataCacheSnapshot configuredSnapshot = SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(
        m_settingsStore != nullptr ? m_settingsStore->loadEphemerisDataCache() : EphemerisDataCacheSnapshot{}
    );

    if (!hasInstalledMetadata(configuredSnapshot)) {
        applyCacheSnapshot(EphemerisDataCacheSnapshot{}, ActiveSource::BundledFallback, true);
        return true;
    }

    const QStringList missingPaths = missingInstalledPaths(configuredSnapshot);
    if (!missingPaths.isEmpty()) {
        qCWarning(skygateEphemerisDataLog).noquote()
            << "Installed ephemeris data paths are missing; falling back to bundled data:" << missingPaths.join(", ");
        applyCacheSnapshot(EphemerisDataCacheSnapshot{}, ActiveSource::MissingInstalledFallback, true);
        return true;
    }

    applyCacheSnapshot(configuredSnapshot, ActiveSource::Installed, true);
    return true;
}

bool SkyEphemerisDataManager::clearInstalledDataCache()
{
    if (m_settingsStore == nullptr || !m_settingsStore->clearEphemerisDataCache()) {
        qCWarning(skygateEphemerisDataLog) << "Unable to clear installed ephemeris data cache metadata";
        return false;
    }

    return restoreFromSettings();
}

bool SkyEphemerisDataManager::clearPlanetaryKernelCache()
{
    if (m_settingsStore == nullptr) {
        qCWarning(skygateEphemerisDataLog) << "Unable to clear planetary kernel cache without settings storage";
        return false;
    }

    EphemerisDataCacheSnapshot snapshot =
        SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(m_settingsStore->loadEphemerisDataCache());
    const QString kernelPath = snapshot.installedKernelPath;
    snapshot.installedKernelAssetId.clear();
    snapshot.installedKernelProfileId.clear();
    snapshot.installedKernelPath.clear();
    snapshot.installedKernelVersion.clear();
    snapshot.installedKernelLongRange.reset();
    snapshot.lastUpdateResult = QStringLiteral("Cleared planetary kernel cache");
    if (!hasInstalledAssetMetadata(snapshot)) {
        snapshot = EphemerisDataCacheSnapshot{};
    }
    if (!m_settingsStore->saveEphemerisDataCache(snapshot)) {
        qCWarning(skygateEphemerisDataLog) << "Unable to persist cleared planetary kernel cache metadata";
        return false;
    }

    if (!removeInstalledFiles(QStringList{kernelPath}, QStringLiteral("planetary kernel"))) {
        return false;
    }

    return restoreFromSettings();
}

bool SkyEphemerisDataManager::clearSupportDataCache()
{
    if (m_settingsStore == nullptr) {
        qCWarning(skygateEphemerisDataLog) << "Unable to clear time and Earth data cache without settings storage";
        return false;
    }

    EphemerisDataCacheSnapshot snapshot =
        SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(m_settingsStore->loadEphemerisDataCache());
    const QStringList supportPaths = EphemerisDataCacheSnapshot::installedSupportDataPaths(snapshot);
    snapshot.installedEarthOrientationPath.clear();
    snapshot.installedEarthOrientationVersion.clear();
    snapshot.installedLeapSecondTablePath.clear();
    snapshot.installedLeapSecondTableVersion.clear();
    snapshot.installedDeltaTDataPath.clear();
    snapshot.installedDeltaTDataVersion.clear();
    snapshot.lastUpdateResult = QStringLiteral("Cleared time and Earth data cache");
    if (!hasInstalledAssetMetadata(snapshot)) {
        snapshot = EphemerisDataCacheSnapshot{};
    }
    if (!m_settingsStore->saveEphemerisDataCache(snapshot)) {
        qCWarning(skygateEphemerisDataLog) << "Unable to persist cleared time and Earth data cache metadata";
        return false;
    }

    if (!removeInstalledFiles(supportPaths, QStringLiteral("time and Earth data"))) {
        return false;
    }

    return restoreFromSettings();
}

void SkyEphemerisDataManager::requestUpdateCancellation() noexcept
{
    m_updateCancellationRequested.store(true);
}

void SkyEphemerisDataManager::clearUpdateCancellation() noexcept
{
    m_updateCancellationRequested.store(false);
}

bool SkyEphemerisDataManager::updateCancellationRequested() const noexcept
{
    return m_updateCancellationRequested.load();
}

void SkyEphemerisDataManager::stageEphemerisUpdateAssetAsync(
    const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
)
{
    m_downloadService->stageAsync(combineCancellation(request, this), std::move(completionHandler));
}

SkyEphemerisDataManager::StagedUpdateActivationResult
SkyEphemerisDataManager::activateVerifiedStagedUpdateSet(const StagedUpdateActivationRequest& request)
{
    StagedUpdateActivationResult result =
        m_cacheController.activate(combineCancellation(request, this), m_activeCacheSnapshot, m_settingsStore);
    if (result.isSuccess()) {
        applyCacheSnapshot(result.cacheSnapshot, ActiveSource::Installed, true);
    }
    return result;
}

void SkyEphemerisDataManager::applyCacheSnapshot(
    EphemerisDataCacheSnapshot cacheSnapshot, const ActiveSource source, const bool emitSignals
)
{
    cacheSnapshot = SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(std::move(cacheSnapshot));
    SkyEphemerisAssetStatusView assetStatusView(cacheSnapshot, source);
    const bool statusChanged = m_assetStatusView.statusText() != assetStatusView.statusText()
                               || m_assetStatusView.datasetInfoText() != assetStatusView.datasetInfoText();
    const bool activeDataDidChange = !cacheSnapshotsEqual(m_activeCacheSnapshot, cacheSnapshot)
                                     || m_activeSource != source || m_activeDataSnapshot == nullptr;

    m_activeCacheSnapshot = std::move(cacheSnapshot);
    m_activeSource = source;
    m_assetStatusView = std::move(assetStatusView);
    if (activeDataDidChange) {
        m_activeDataSnapshot = std::make_shared<SkyEphemerisSnapshotAdapter>(
            m_activeCacheSnapshot,
            m_activeSource == ActiveSource::Installed,
            m_bundledFallbackManifest,
            m_bundledFallbackResourceRoot,
            m_bundledFallbackProfileId
        );
        ++m_dataRevision;
    }

    if (!emitSignals) {
        return;
    }
    if (statusChanged) {
        emit statusTextChanged();
    }
    if (activeDataDidChange) {
        emit dataRevisionChanged();
        emit activeDataChanged();
    }
}
