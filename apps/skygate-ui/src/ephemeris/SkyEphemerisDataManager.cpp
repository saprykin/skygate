#include "SkyEphemerisDataManager.hpp"

#include "SkyEphemerisCacheController.hpp"
#include "SkyEphemerisDownloadService.hpp"
#include "SkyEphemerisSnapshotAdapter.hpp"
#include "SkySettingsStore.hpp"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStringList>

#include <memory>
#include <utility>

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisDataLog, "skygate.ephemeris.data")

using EphemerisDataCacheSnapshot = SkySettingsStore::EphemerisDataCacheSnapshot;
using StagedUpdateActivationRequest = SkyEphemerisDataManager::StagedUpdateActivationRequest;
using StagedUpdateDownloadRequest = SkyEphemerisDataManager::StagedUpdateDownloadRequest;

QString trimmed(const QString& value)
{
    return value.trimmed();
}

EphemerisDataCacheSnapshot normalizedSnapshot(EphemerisDataCacheSnapshot snapshot)
{
    snapshot.installedKernelAssetId = trimmed(snapshot.installedKernelAssetId);
    snapshot.installedKernelProfileId = trimmed(snapshot.installedKernelProfileId);
    if (snapshot.installedKernelProfileId == QStringLiteral("modern")) {
        snapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    }
    snapshot.installedKernelPath = trimmed(snapshot.installedKernelPath);
    snapshot.installedKernelVersion = trimmed(snapshot.installedKernelVersion);
    snapshot.installedEarthOrientationPath = trimmed(snapshot.installedEarthOrientationPath);
    snapshot.installedEarthOrientationVersion = trimmed(snapshot.installedEarthOrientationVersion);
    snapshot.installedLeapSecondTablePath = trimmed(snapshot.installedLeapSecondTablePath);
    snapshot.installedLeapSecondTableVersion = trimmed(snapshot.installedLeapSecondTableVersion);
    snapshot.installedDeltaTDataPath = trimmed(snapshot.installedDeltaTDataPath);
    snapshot.installedDeltaTDataVersion = trimmed(snapshot.installedDeltaTDataVersion);
    snapshot.dataRevisionToken = trimmed(snapshot.dataRevisionToken);
    snapshot.lastUpdateResult = trimmed(snapshot.lastUpdateResult);
    if (snapshot.dataRevisionToken.isEmpty()) {
        snapshot.dataRevisionToken = EphemerisDataCacheSnapshot{}.dataRevisionToken;
    }
    if (snapshot.lastUpdateResult.isEmpty()) {
        snapshot.lastUpdateResult = EphemerisDataCacheSnapshot{}.lastUpdateResult;
    }
    return snapshot;
}

bool hasInstalledMetadata(const EphemerisDataCacheSnapshot& snapshot)
{
    return !snapshot.installedKernelAssetId.isEmpty() || !snapshot.installedKernelProfileId.isEmpty()
           || !snapshot.installedKernelPath.isEmpty() || !snapshot.installedKernelVersion.isEmpty()
           || !snapshot.installedEarthOrientationPath.isEmpty() || !snapshot.installedEarthOrientationVersion.isEmpty()
           || !snapshot.installedLeapSecondTablePath.isEmpty() || !snapshot.installedLeapSecondTableVersion.isEmpty()
           || !snapshot.installedDeltaTDataPath.isEmpty() || !snapshot.installedDeltaTDataVersion.isEmpty()
           || snapshot.dataRevisionToken != EphemerisDataCacheSnapshot{}.dataRevisionToken;
}

bool hasInstalledAssetMetadata(const EphemerisDataCacheSnapshot& snapshot)
{
    return !snapshot.installedKernelAssetId.isEmpty() || !snapshot.installedKernelProfileId.isEmpty()
           || !snapshot.installedKernelPath.isEmpty() || !snapshot.installedKernelVersion.isEmpty()
           || !snapshot.installedEarthOrientationPath.isEmpty() || !snapshot.installedEarthOrientationVersion.isEmpty()
           || !snapshot.installedLeapSecondTablePath.isEmpty() || !snapshot.installedLeapSecondTableVersion.isEmpty()
           || !snapshot.installedDeltaTDataPath.isEmpty() || !snapshot.installedDeltaTDataVersion.isEmpty();
}

QStringList missingInstalledPaths(const EphemerisDataCacheSnapshot& snapshot)
{
    QStringList missingPaths;
    const QStringList candidatePaths{
        snapshot.installedKernelPath,
        snapshot.installedEarthOrientationPath,
        snapshot.installedLeapSecondTablePath,
        snapshot.installedDeltaTDataPath,
    };
    for (const QString& path : candidatePaths) {
        if (!path.isEmpty() && !QFileInfo::exists(path)) {
            missingPaths.push_back(path);
        }
    }
    return missingPaths;
}

bool cacheSnapshotsEqual(const EphemerisDataCacheSnapshot& lhs, const EphemerisDataCacheSnapshot& rhs)
{
    return lhs.installedKernelAssetId == rhs.installedKernelAssetId
           && lhs.installedKernelProfileId == rhs.installedKernelProfileId
           && lhs.installedKernelPath == rhs.installedKernelPath
           && lhs.installedKernelVersion == rhs.installedKernelVersion
           && lhs.installedEarthOrientationPath == rhs.installedEarthOrientationPath
           && lhs.installedEarthOrientationVersion == rhs.installedEarthOrientationVersion
           && lhs.installedLeapSecondTablePath == rhs.installedLeapSecondTablePath
           && lhs.installedLeapSecondTableVersion == rhs.installedLeapSecondTableVersion
           && lhs.installedDeltaTDataPath == rhs.installedDeltaTDataPath
           && lhs.installedDeltaTDataVersion == rhs.installedDeltaTDataVersion
           && lhs.dataRevisionToken == rhs.dataRevisionToken && lhs.lastUpdateResult == rhs.lastUpdateResult;
}

QString installedDatasetInfoText(const EphemerisDataCacheSnapshot& snapshot)
{
    QStringList parts;
    if (!snapshot.installedKernelVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Kernel %1").arg(snapshot.installedKernelVersion));
    }
    if (!snapshot.installedEarthOrientationVersion.isEmpty()) {
        parts.push_back(QStringLiteral("EOP %1").arg(snapshot.installedEarthOrientationVersion));
    }
    if (!snapshot.installedLeapSecondTableVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Leap seconds %1").arg(snapshot.installedLeapSecondTableVersion));
    }
    if (!snapshot.installedDeltaTDataVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Delta T %1").arg(snapshot.installedDeltaTDataVersion));
    }
    if (parts.isEmpty()) {
        parts.push_back(QStringLiteral("Installed metadata"));
    }
    return parts.join(QStringLiteral(" | "));
}

bool installedKernelLooksLongRange(const EphemerisDataCacheSnapshot& snapshot)
{
    const QString haystack =
        QStringList{
            snapshot.installedKernelAssetId,
            snapshot.installedKernelProfileId,
            snapshot.installedKernelVersion,
        }
            .join(QLatin1Char(' '))
            .toLower();
    return haystack.contains(QStringLiteral("de441")) || haystack.contains(QStringLiteral("long"));
}

StagedUpdateDownloadRequest
combineDownloadCancellation(StagedUpdateDownloadRequest request, const SkyEphemerisDataManager* manager)
{
    const auto original = request.cancellationRequested;
    request.cancellationRequested = [manager, original] {
        return manager->updateCancellationRequested() || (original != nullptr && original());
    };
    return request;
}

StagedUpdateActivationRequest
combineActivationCancellation(StagedUpdateActivationRequest request, const SkyEphemerisDataManager* manager)
{
    const auto original = request.cancellationRequested;
    request.cancellationRequested = [manager, original] {
        return manager->updateCancellationRequested() || (original != nullptr && original());
    };
    return request;
}

}  // namespace

SkyEphemerisDataManager::SkyEphemerisDataManager(SkySettingsStore* settingsStore, QObject* parent)
    : QObject(parent), m_settingsStore(settingsStore),
      m_downloadService(std::make_unique<SkyEphemerisDownloadService>(nullptr, this))
{
    static_cast<void>(restoreFromSettings());
}

SkyEphemerisDataManager::~SkyEphemerisDataManager() = default;

QString SkyEphemerisDataManager::statusText() const
{
    return m_statusText;
}

QString SkyEphemerisDataManager::datasetInfoText() const
{
    return m_datasetInfoText;
}

QString SkyEphemerisDataManager::shortRangeKernelStatusText() const
{
    if (usingInstalledData() && !m_activeCacheSnapshot.installedKernelVersion.isEmpty()) {
        return QStringLiteral("Installed: %1").arg(m_activeCacheSnapshot.installedKernelVersion);
    }
    if (m_activeSource == ActiveSource::MissingInstalledFallback) {
        return QStringLiteral("Bundled fallback (installed data missing)");
    }
    return QStringLiteral("Bundled fallback");
}

QString SkyEphemerisDataManager::longRangeKernelStatusText() const
{
    if (usingInstalledData() && installedKernelLooksLongRange(m_activeCacheSnapshot)) {
        const QString version = m_activeCacheSnapshot.installedKernelVersion.isEmpty()
                                    ? QStringLiteral("installed")
                                    : m_activeCacheSnapshot.installedKernelVersion;
        return QStringLiteral("Installed: %1").arg(version);
    }
    return QStringLiteral("Not installed");
}

QString SkyEphemerisDataManager::earthOrientationStatusText() const
{
    if (usingInstalledData() && !m_activeCacheSnapshot.installedEarthOrientationVersion.isEmpty()) {
        return QStringLiteral("Installed: %1").arg(m_activeCacheSnapshot.installedEarthOrientationVersion);
    }
    if (m_activeSource == ActiveSource::MissingInstalledFallback) {
        return QStringLiteral("Bundled fallback (installed data missing)");
    }
    return QStringLiteral("Bundled fallback");
}

QString SkyEphemerisDataManager::leapSecondStatusText() const
{
    if (usingInstalledData() && !m_activeCacheSnapshot.installedLeapSecondTableVersion.isEmpty()) {
        return QStringLiteral("Installed: %1").arg(m_activeCacheSnapshot.installedLeapSecondTableVersion);
    }
    if (m_activeSource == ActiveSource::MissingInstalledFallback) {
        return QStringLiteral("Bundled fallback (installed data missing)");
    }
    return QStringLiteral("Bundled fallback");
}

QString SkyEphemerisDataManager::deltaTStatusText() const
{
    if (usingInstalledData() && !m_activeCacheSnapshot.installedDeltaTDataVersion.isEmpty()) {
        return QStringLiteral("Installed: %1").arg(m_activeCacheSnapshot.installedDeltaTDataVersion);
    }
    if (m_activeSource == ActiveSource::MissingInstalledFallback) {
        return QStringLiteral("Bundled fallback (installed data missing)");
    }
    return QStringLiteral("Bundled fallback");
}

std::uint64_t SkyEphemerisDataManager::planetaryKernelCacheSizeBytes() const
{
    const QFileInfo kernelFileInfo(m_activeCacheSnapshot.installedKernelPath);
    if (!kernelFileInfo.exists() || !kernelFileInfo.isFile() || kernelFileInfo.size() <= 0) {
        return 0U;
    }
    return static_cast<std::uint64_t>(kernelFileInfo.size());
}

std::uint64_t SkyEphemerisDataManager::supportDataCacheSizeBytes() const
{
    std::uint64_t totalBytes = 0U;
    for (const QString& path : {
             m_activeCacheSnapshot.installedEarthOrientationPath,
             m_activeCacheSnapshot.installedLeapSecondTablePath,
             m_activeCacheSnapshot.installedDeltaTDataPath,
         }) {
        const QFileInfo fileInfo(path);
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0) {
            totalBytes += static_cast<std::uint64_t>(fileInfo.size());
        }
    }
    return totalBytes;
}

QString SkyEphemerisDataManager::lastUpdateResultText() const
{
    return m_activeCacheSnapshot.lastUpdateResult;
}

QString SkyEphemerisDataManager::dataRevisionToken() const
{
    return m_activeCacheSnapshot.dataRevisionToken;
}

bool SkyEphemerisDataManager::usingInstalledData() const noexcept
{
    return m_activeSource == ActiveSource::Installed;
}

std::uint64_t SkyEphemerisDataManager::dataRevision() const noexcept
{
    return m_dataRevision;
}

SkySettingsStore::EphemerisDataCacheSnapshot SkyEphemerisDataManager::activeCacheSnapshot() const
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
        applyCacheSnapshot(m_activeCacheSnapshot, m_activeSource, m_statusText, true);
    }
}

bool SkyEphemerisDataManager::restoreFromSettings()
{
    const EphemerisDataCacheSnapshot configuredSnapshot = normalizedSnapshot(
        m_settingsStore != nullptr ? m_settingsStore->loadEphemerisDataCache() : EphemerisDataCacheSnapshot{}
    );

    if (!hasInstalledMetadata(configuredSnapshot)) {
        applyCacheSnapshot(
            EphemerisDataCacheSnapshot{},
            ActiveSource::BundledFallback,
            QStringLiteral("Ephemeris data: Bundled fallback"),
            true
        );
        return true;
    }

    const QStringList missingPaths = missingInstalledPaths(configuredSnapshot);
    if (!missingPaths.isEmpty()) {
        qCWarning(skygateEphemerisDataLog).noquote()
            << "Installed ephemeris data paths are missing; falling back to bundled data:" << missingPaths.join(", ");
        applyCacheSnapshot(
            EphemerisDataCacheSnapshot{},
            ActiveSource::MissingInstalledFallback,
            QStringLiteral("Ephemeris data: Installed data missing; bundled fallback active"),
            true
        );
        return true;
    }

    applyCacheSnapshot(
        configuredSnapshot, ActiveSource::Installed, QStringLiteral("Ephemeris data: Installed data active"), true
    );
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

    EphemerisDataCacheSnapshot snapshot = normalizedSnapshot(m_settingsStore->loadEphemerisDataCache());
    const QString kernelPath = snapshot.installedKernelPath;
    snapshot.installedKernelAssetId.clear();
    snapshot.installedKernelProfileId.clear();
    snapshot.installedKernelPath.clear();
    snapshot.installedKernelVersion.clear();
    snapshot.lastUpdateResult = QStringLiteral("Cleared planetary kernel cache");
    if (!hasInstalledAssetMetadata(snapshot)) {
        snapshot = EphemerisDataCacheSnapshot{};
    }
    if (!m_settingsStore->saveEphemerisDataCache(snapshot)) {
        qCWarning(skygateEphemerisDataLog) << "Unable to persist cleared planetary kernel cache metadata";
        return false;
    }

    if (!kernelPath.isEmpty()) {
        QFile kernelFile(kernelPath);
        if (kernelFile.exists() && !kernelFile.remove()) {
            qCWarning(skygateEphemerisDataLog).noquote()
                << "Unable to remove planetary kernel cache file" << kernelPath << kernelFile.errorString();
            return false;
        }
    }

    return restoreFromSettings();
}

bool SkyEphemerisDataManager::clearSupportDataCache()
{
    if (m_settingsStore == nullptr) {
        qCWarning(skygateEphemerisDataLog) << "Unable to clear time and Earth data cache without settings storage";
        return false;
    }

    EphemerisDataCacheSnapshot snapshot = normalizedSnapshot(m_settingsStore->loadEphemerisDataCache());
    const QStringList supportPaths{
        snapshot.installedEarthOrientationPath,
        snapshot.installedLeapSecondTablePath,
        snapshot.installedDeltaTDataPath,
    };
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

    for (const QString& path : supportPaths) {
        if (path.isEmpty()) {
            continue;
        }
        QFile file(path);
        if (file.exists() && !file.remove()) {
            qCWarning(skygateEphemerisDataLog).noquote()
                << "Unable to remove time and Earth data cache file" << path << file.errorString();
            return false;
        }
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

SkyEphemerisDataManager::StagedUpdateDownloadResult
SkyEphemerisDataManager::stageEphemerisUpdateAsset(const StagedUpdateDownloadRequest& request)
{
    return m_downloadService->stage(combineDownloadCancellation(request, this));
}

void SkyEphemerisDataManager::stageEphemerisUpdateAssetAsync(
    const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
)
{
    m_downloadService->stageAsync(combineDownloadCancellation(request, this), std::move(completionHandler));
}

SkyEphemerisDataManager::StagedUpdateActivationResult
SkyEphemerisDataManager::activateVerifiedStagedUpdateSet(const StagedUpdateActivationRequest& request)
{
    StagedUpdateActivationResult result = m_cacheController.activate(
        combineActivationCancellation(request, this), m_activeCacheSnapshot, m_settingsStore
    );
    if (result.isSuccess()) {
        applyCacheSnapshot(
            result.cacheSnapshot, ActiveSource::Installed, QStringLiteral("Ephemeris data: Installed data active"), true
        );
    }
    return result;
}

void SkyEphemerisDataManager::applyCacheSnapshot(
    EphemerisDataCacheSnapshot cacheSnapshot, const ActiveSource source, QString statusText, const bool emitSignals
)
{
    cacheSnapshot = normalizedSnapshot(std::move(cacheSnapshot));
    QString datasetInfoText;
    switch (source) {
    case ActiveSource::Installed:
        datasetInfoText = installedDatasetInfoText(cacheSnapshot);
        break;
    case ActiveSource::MissingInstalledFallback:
        datasetInfoText = QStringLiteral("Bundled fallback (installed data missing)");
        break;
    case ActiveSource::BundledFallback:
        datasetInfoText = QStringLiteral("Bundled fallback");
        break;
    }

    const bool statusChanged = m_statusText != statusText || m_datasetInfoText != datasetInfoText;
    const bool activeDataDidChange = !cacheSnapshotsEqual(m_activeCacheSnapshot, cacheSnapshot)
                                     || m_activeSource != source || m_activeDataSnapshot == nullptr;

    m_activeCacheSnapshot = std::move(cacheSnapshot);
    m_activeSource = source;
    m_statusText = std::move(statusText);
    m_datasetInfoText = std::move(datasetInfoText);
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
