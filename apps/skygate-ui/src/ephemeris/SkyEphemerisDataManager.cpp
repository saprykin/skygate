#include "SkyEphemerisDataManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

using EphemerisDataCacheSnapshot = SkySettingsStore::EphemerisDataCacheSnapshot;
using skygate::ephemeris::EphemerisDataActivationRequest;
using skygate::ephemeris::EphemerisDataManifest;
using skygate::ephemeris::EphemerisDataManifestAsset;
using skygate::ephemeris::EphemerisDataManifestAssetKind;
using skygate::ephemeris::EphemerisDataManifestProfile;
using skygate::ephemeris::EphemerisStagedUpdateVerificationRequest;
using skygate::ephemeris::EphemerisStagedUpdateVerificationResult;
using skygate::ephemeris::EphemerisStagedUpdateVerificationStatus;
using skygate::ephemeris::EphemerisTextDataAsset;
using skygate::ephemeris::IEphemerisDataSnapshot;
using KernelDataAsset = skygate::ephemeris::EphemerisKernelDataAsset;

constexpr std::size_t kDownloadBufferBytes = 64U * 1024U;

QString trimmed(const QString& value)
{
    return value.trimmed();
}

EphemerisDataCacheSnapshot normalizedSnapshot(EphemerisDataCacheSnapshot snapshot)
{
    snapshot.installedKernelAssetId = trimmed(snapshot.installedKernelAssetId);
    snapshot.installedKernelProfileId = trimmed(snapshot.installedKernelProfileId);
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

std::filesystem::path pathFromQString(const QString& path)
{
    return std::filesystem::path(path.toStdString());
}

QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

QString stringToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString safePathSegment(QString value)
{
    value = value.trimmed();
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        if (character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_')
            || character == QLatin1Char('.')) {
            result.push_back(character);
        } else {
            result.push_back(QLatin1Char('_'));
        }
    }
    while (result.contains(QStringLiteral(".."))) {
        result.replace(QStringLiteral(".."), QStringLiteral("."));
    }
    if (result.isEmpty() || result == QStringLiteral(".")) {
        return QStringLiteral("update");
    }
    return result;
}

QString defaultRevisionToken(const EphemerisDataManifest& manifest, const EphemerisDataManifestProfile& profile)
{
    QStringList parts;
    if (!manifest.dataSetInfo.id.empty()) {
        parts.push_back(stringToQString(manifest.dataSetInfo.id));
    }
    if (!manifest.dataSetInfo.version.empty()) {
        parts.push_back(stringToQString(manifest.dataSetInfo.version));
    }
    if (!profile.id.empty()) {
        parts.push_back(stringToQString(profile.id));
    }
    return safePathSegment(parts.isEmpty() ? QStringLiteral("update") : parts.join(QLatin1Char('-')));
}

void addDiagnostic(SkyEphemerisDataManager::StagedUpdateActivationResult& result, QString diagnostic)
{
    if (!diagnostic.trimmed().isEmpty()) {
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

void addDiagnostic(SkyEphemerisDataManager::StagedUpdateDownloadResult& result, QString diagnostic)
{
    if (!diagnostic.trimmed().isEmpty()) {
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

void addDiagnostics(
    SkyEphemerisDataManager::StagedUpdateActivationResult& result, const std::vector<std::string>& diagnostics
)
{
    for (const std::string& diagnostic : diagnostics) {
        addDiagnostic(result, stringToQString(diagnostic));
    }
}

bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(SkyEphemerisDataManager::StagedUpdateActivationResult& result, QString diagnostic)
{
    result.status = SkyEphemerisDataManager::StagedUpdateActivationStatus::Canceled;
    result.verificationStatus = EphemerisStagedUpdateVerificationStatus::Canceled;
    addDiagnostic(result, std::move(diagnostic));
}

void markCanceled(SkyEphemerisDataManager::StagedUpdateDownloadResult& result, QString diagnostic)
{
    result.status = SkyEphemerisDataManager::StagedUpdateDownloadStatus::Canceled;
    addDiagnostic(result, std::move(diagnostic));
}

[[nodiscard]] bool
cacheRootContainsActivePath(const std::filesystem::path& root, const EphemerisDataCacheSnapshot& activeSnapshot);

void cleanupInactiveActivationRoot(
    const std::filesystem::path& activationRoot,
    const EphemerisDataCacheSnapshot& activeSnapshot,
    SkyEphemerisDataManager::StagedUpdateActivationResult& result
)
{
    if (activationRoot.empty() || cacheRootContainsActivePath(activationRoot, activeSnapshot)) {
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(activationRoot, error);
    if (error) {
        addDiagnostic(
            result,
            QStringLiteral("Unable to clean failed ephemeris activation cache: %1")
                .arg(QString::fromStdString(error.message()))
        );
    }
}

void cleanupCanceledStagingRoot(
    const SkyEphemerisDataManager::StagedUpdateActivationRequest& request,
    const EphemerisDataCacheSnapshot& activeSnapshot,
    SkyEphemerisDataManager::StagedUpdateActivationResult& result
)
{
    if (request.retainStagedResourcesOnCancellation || request.stagedResourceRoot.trimmed().isEmpty()) {
        return;
    }

    const std::filesystem::path stagedRoot = pathFromQString(request.stagedResourceRoot);
    if (cacheRootContainsActivePath(stagedRoot, activeSnapshot)) {
        addDiagnostic(
            result, QStringLiteral("Canceled ephemeris staging root was retained because it contains active data.")
        );
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(stagedRoot, error);
    if (error) {
        addDiagnostic(
            result,
            QStringLiteral("Unable to clean canceled ephemeris staging root: %1")
                .arg(QString::fromStdString(error.message()))
        );
    }
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

std::optional<EphemerisTextDataAsset>
loadTextAsset(const QString& path, const QString& id, const QString& version, const QString& provenance)
{
    if (path.isEmpty()) {
        return std::nullopt;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    EphemerisTextDataAsset asset;
    asset.id = id.toStdString();
    asset.version = version.toStdString();
    asset.provenance = provenance.toStdString();
    asset.content = QString::fromUtf8(file.readAll()).toStdString();
    return asset;
}

bool pathContains(const std::filesystem::path& root, const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    const std::filesystem::path normalizedRoot = root.lexically_normal();
    const std::filesystem::path normalizedPath = pathFromQString(path).lexically_normal();
    auto rootIt = normalizedRoot.begin();
    auto pathIt = normalizedPath.begin();
    for (; rootIt != normalizedRoot.end() && pathIt != normalizedPath.end(); ++rootIt, ++pathIt) {
        if (*rootIt != *pathIt) {
            return false;
        }
    }
    return rootIt == normalizedRoot.end();
}

[[nodiscard]] bool hasUnsafePathComponent(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute()) {
        return true;
    }

    for (const std::filesystem::path& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return true;
        }
    }

    return false;
}

bool cacheRootContainsActivePath(const std::filesystem::path& root, const EphemerisDataCacheSnapshot& activeSnapshot)
{
    return pathContains(root, activeSnapshot.installedKernelPath)
           || pathContains(root, activeSnapshot.installedEarthOrientationPath)
           || pathContains(root, activeSnapshot.installedLeapSecondTablePath)
           || pathContains(root, activeSnapshot.installedDeltaTDataPath);
}

std::filesystem::path activationCacheRoot(
    const std::filesystem::path& writableCacheRoot,
    const QString& revisionToken,
    const EphemerisDataCacheSnapshot& activeSnapshot
)
{
    const std::filesystem::path updatesRoot = writableCacheRoot / "updates";
    const std::filesystem::path baseRoot = updatesRoot / revisionToken.toStdString();
    if (!cacheRootContainsActivePath(baseRoot, activeSnapshot)) {
        return baseRoot;
    }

    for (int suffix = 1; suffix < 1000; ++suffix) {
        const QString candidateToken = suffix == 1 ? QStringLiteral("%1-activation").arg(revisionToken)
                                                   : QStringLiteral("%1-activation-%2").arg(revisionToken).arg(suffix);
        std::filesystem::path candidateRoot = updatesRoot / candidateToken.toStdString();
        if (!cacheRootContainsActivePath(candidateRoot, activeSnapshot)) {
            return candidateRoot;
        }
    }

    return updatesRoot / QStringLiteral("%1-activation-overflow").arg(revisionToken).toStdString();
}

EphemerisDataCacheSnapshot cacheSnapshotForActivatedProfile(
    const EphemerisDataManifest& manifest,
    const EphemerisDataManifestProfile& profile,
    const std::vector<std::pair<std::string, std::filesystem::path>>& activePaths,
    const QString& revisionToken
)
{
    EphemerisDataCacheSnapshot snapshot;
    snapshot.dataRevisionToken = revisionToken;
    snapshot.lastUpdateResult =
        QStringLiteral("Installed %1")
            .arg(profile.displayName.empty() ? stringToQString(profile.id) : stringToQString(profile.displayName));

    for (const std::string& assetId : profile.assetIds) {
        const EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        if (asset == nullptr) {
            continue;
        }
        const auto activePath = std::find_if(activePaths.begin(), activePaths.end(), [asset](const auto& entry) {
            return entry.first == asset->id;
        });

        switch (asset->kind) {
        case EphemerisDataManifestAssetKind::SolarSystemKernel:
            snapshot.installedKernelAssetId = stringToQString(asset->id);
            snapshot.installedKernelProfileId = stringToQString(asset->profileId);
            snapshot.installedKernelVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedKernelPath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifestAssetKind::EarthOrientationData:
            snapshot.installedEarthOrientationVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedEarthOrientationPath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifestAssetKind::LeapSecondTable:
            snapshot.installedLeapSecondTableVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedLeapSecondTablePath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifestAssetKind::DeltaTData:
            snapshot.installedDeltaTDataVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedDeltaTDataPath = pathToQString(activePath->second);
            }
            break;
        }
    }

    return snapshot;
}

class SkyActiveEphemerisDataSnapshot final : public IEphemerisDataSnapshot {
public:
    SkyActiveEphemerisDataSnapshot(EphemerisDataCacheSnapshot cacheSnapshot, const bool installedDataActive)
        : m_cacheSnapshot(std::move(cacheSnapshot)), m_installedDataActive(installedDataActive)
    {
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        if (!m_installedDataActive) {
            return std::nullopt;
        }

        return loadTextAsset(
            m_cacheSnapshot.installedLeapSecondTablePath,
            QStringLiteral("installed-leap-second-table"),
            m_cacheSnapshot.installedLeapSecondTableVersion,
            QStringLiteral("Installed ephemeris data cache")
        );
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> deltaTDataAsset() const override
    {
        if (!m_installedDataActive) {
            return std::nullopt;
        }

        return loadTextAsset(
            m_cacheSnapshot.installedDeltaTDataPath,
            QStringLiteral("installed-delta-t-data"),
            m_cacheSnapshot.installedDeltaTDataVersion,
            QStringLiteral("Installed ephemeris data cache")
        );
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> earthOrientationDataAsset() const override
    {
        if (!m_installedDataActive) {
            return std::nullopt;
        }

        return loadTextAsset(
            m_cacheSnapshot.installedEarthOrientationPath,
            QStringLiteral("installed-earth-orientation"),
            m_cacheSnapshot.installedEarthOrientationVersion,
            QStringLiteral("Installed ephemeris data cache")
        );
    }

    [[nodiscard]] std::optional<KernelDataAsset> solarSystemKernelAsset(std::string_view assetId) const override
    {
        if (!m_installedDataActive || m_cacheSnapshot.installedKernelPath.isEmpty()) {
            return std::nullopt;
        }
        const QString requestedAssetId = QString::fromUtf8(assetId.data(), static_cast<qsizetype>(assetId.size()));
        if (m_cacheSnapshot.installedKernelAssetId.isEmpty()
            || m_cacheSnapshot.installedKernelAssetId != requestedAssetId) {
            return std::nullopt;
        }

        KernelDataAsset asset;
        asset.id = m_cacheSnapshot.installedKernelAssetId.toStdString();
        asset.profileId = m_cacheSnapshot.installedKernelProfileId.toStdString();
        asset.version = m_cacheSnapshot.installedKernelVersion.toStdString();
        asset.provenance = "Installed ephemeris data cache";
        asset.activePath = m_cacheSnapshot.installedKernelPath.toStdString();
        return asset;
    }

private:
    EphemerisDataCacheSnapshot m_cacheSnapshot;
    bool m_installedDataActive = false;
};

}  // namespace

SkyEphemerisDataManager::SkyEphemerisDataManager(SkySettingsStore* settingsStore, QObject* parent)
    : QObject(parent), m_settingsStore(settingsStore)
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

QString SkyEphemerisDataManager::modernKernelStatusText() const
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

SkyEphemerisDataManager::StagedUpdateDownloadResult
SkyEphemerisDataManager::stageEphemerisUpdateAsset(const StagedUpdateDownloadRequest& request)
{
    StagedUpdateDownloadResult result;
    const auto isCanceled = [this, &request] {
        return m_updateCancellationRequested.load() || cancellationRequested(request.cancellationRequested);
    };
    const auto cleanupPartialStaging = [&request, &result] {
        if (request.retainPartialStagingOnCancellation || result.stagedPath.trimmed().isEmpty()) {
            return;
        }
        if (!QFile::remove(result.stagedPath) && QFileInfo::exists(result.stagedPath)) {
            addDiagnostic(result, QStringLiteral("Unable to clean canceled ephemeris staged download file."));
        }
    };

    if (isCanceled()) {
        markCanceled(result, QStringLiteral("Ephemeris update download was canceled before transfer."));
        return result;
    }
    if (request.asset == nullptr) {
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires an asset."));
        return result;
    }
    if (request.sourceResourceRoot.trimmed().isEmpty()) {
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires a source root."));
        return result;
    }
    if (request.stagedResourceRoot.trimmed().isEmpty()) {
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires a staging root."));
        return result;
    }

    const std::filesystem::path relativePath(request.asset->relativePath);
    if (hasUnsafePathComponent(relativePath)) {
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires a safe relative asset path."));
        return result;
    }

    const std::filesystem::path sourcePath = pathFromQString(request.sourceResourceRoot) / relativePath;
    const std::filesystem::path stagedPath = pathFromQString(request.stagedResourceRoot) / relativePath;
    result.stagedPath = pathToQString(stagedPath);

    QFile sourceFile(pathToQString(sourcePath));
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = StagedUpdateDownloadStatus::MissingSource;
        addDiagnostic(result, QStringLiteral("Ephemeris update download source file is missing."));
        return result;
    }

    const QFileInfo stagedFileInfo(result.stagedPath);
    if (!QDir().mkpath(stagedFileInfo.absolutePath())) {
        result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(result, QStringLiteral("Unable to create ephemeris update staging directory."));
        return result;
    }

    QFile stagedFile(result.stagedPath);
    if (!stagedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(result, QStringLiteral("Unable to create ephemeris update staging file."));
        return result;
    }

    std::array<char, kDownloadBufferBytes> buffer{};
    while (!sourceFile.atEnd()) {
        if (isCanceled()) {
            stagedFile.close();
            markCanceled(result, QStringLiteral("Ephemeris update download was canceled during transfer."));
            cleanupPartialStaging();
            return result;
        }

        const qint64 bytesRead = sourceFile.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(result, QStringLiteral("Unable to read ephemeris update source file."));
            return result;
        }
        if (bytesRead == 0) {
            continue;
        }
        if (stagedFile.write(buffer.data(), bytesRead) != bytesRead) {
            result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(result, QStringLiteral("Unable to write ephemeris update staging file."));
            return result;
        }
        result.stagedBytes += static_cast<std::uint64_t>(bytesRead);
    }

    if (isCanceled()) {
        stagedFile.close();
        markCanceled(result, QStringLiteral("Ephemeris update download was canceled after transfer."));
        cleanupPartialStaging();
        return result;
    }
    if (!stagedFile.flush()) {
        result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(result, QStringLiteral("Unable to flush ephemeris update staging file."));
        return result;
    }

    result.status = StagedUpdateDownloadStatus::Downloaded;
    return result;
}

SkyEphemerisDataManager::StagedUpdateActivationResult
SkyEphemerisDataManager::activateVerifiedStagedUpdateSet(const StagedUpdateActivationRequest& request)
{
    StagedUpdateActivationResult result;
    const auto isCanceled = [this, &request] {
        return m_updateCancellationRequested.load() || cancellationRequested(request.cancellationRequested);
    };
    if (isCanceled()) {
        markCanceled(result, QStringLiteral("Ephemeris staged update activation was canceled before verification."));
        cleanupCanceledStagingRoot(request, m_activeCacheSnapshot, result);
        return result;
    }
    if (request.manifest == nullptr) {
        addDiagnostic(result, QStringLiteral("Ephemeris staged update activation requires a manifest."));
        return result;
    }
    const std::string profileId = request.profileId.trimmed().toStdString();
    if (profileId.empty()) {
        addDiagnostic(result, QStringLiteral("Ephemeris staged update activation requires a profile id."));
        return result;
    }
    if (request.stagedResourceRoot.trimmed().isEmpty()) {
        addDiagnostic(result, QStringLiteral("Ephemeris staged update activation requires a staging root."));
        return result;
    }
    if (request.writableCacheRoot.trimmed().isEmpty()) {
        addDiagnostic(result, QStringLiteral("Ephemeris staged update activation requires a writable cache root."));
        return result;
    }

    EphemerisStagedUpdateVerificationRequest verificationRequest{
        .manifest = request.manifest,
        .profileId = profileId,
        .stagedResourceRoot = pathFromQString(request.stagedResourceRoot),
        .requiredKinds = request.requiredKinds,
        .expectedComponents = request.expectedComponents,
        .cancellationRequested = isCanceled,
    };

    const EphemerisStagedUpdateVerificationResult verificationResult =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest);
    result.verificationStatus = verificationResult.status;
    if (verificationResult.status == EphemerisStagedUpdateVerificationStatus::Canceled) {
        result.status = StagedUpdateActivationStatus::Canceled;
        addDiagnostics(result, verificationResult.diagnostics);
        if (result.diagnostics.empty()) {
            addDiagnostic(result, QStringLiteral("Ephemeris staged update verification was canceled."));
        }
        cleanupCanceledStagingRoot(request, m_activeCacheSnapshot, result);
        return result;
    }
    if (!verificationResult.isSuccess()) {
        result.status = StagedUpdateActivationStatus::VerificationFailed;
        addDiagnostics(result, verificationResult.diagnostics);
        return result;
    }
    if (isCanceled()) {
        result.status = StagedUpdateActivationStatus::Canceled;
        addDiagnostic(result, QStringLiteral("Ephemeris staged update activation was canceled before install."));
        cleanupCanceledStagingRoot(request, m_activeCacheSnapshot, result);
        return result;
    }

    const EphemerisDataManifestProfile* profile = request.manifest->profile(profileId);
    if (profile == nullptr) {
        result.status = StagedUpdateActivationStatus::VerificationFailed;
        result.verificationStatus = EphemerisStagedUpdateVerificationStatus::UnsupportedProfile;
        addDiagnostic(result, QStringLiteral("Verified ephemeris staged update profile is no longer available."));
        return result;
    }

    const QString revisionToken = safePathSegment(
        request.revisionToken.trimmed().isEmpty() ? defaultRevisionToken(*request.manifest, *profile)
                                                  : request.revisionToken
    );
    const std::filesystem::path revisionCacheRoot =
        activationCacheRoot(pathFromQString(request.writableCacheRoot), revisionToken, m_activeCacheSnapshot);

    std::vector<std::pair<std::string, std::filesystem::path>> activePaths;
    activePaths.reserve(profile->assetIds.size());
    for (const std::string& assetId : profile->assetIds) {
        const EphemerisDataManifestAsset* asset = request.manifest->asset(assetId);
        if (asset == nullptr) {
            result.status = StagedUpdateActivationStatus::ActivationFailed;
            addDiagnostic(result, QStringLiteral("Verified ephemeris staged update references a missing asset."));
            return result;
        }

        const EphemerisDataActivationRequest activationRequest{
            .asset = asset,
            .bundledResourceRoot = pathFromQString(request.stagedResourceRoot),
            .writableCacheRoot = revisionCacheRoot,
            .allowQtResourceKernelAssets = request.allowQtResourceKernelAssets,
            .largeKernelResourceThresholdBytes = request.largeKernelResourceThresholdBytes,
            .cancellationRequested = isCanceled,
        };
        const skygate::ephemeris::EphemerisDataActivationResult activationResult =
            skygate::ephemeris::activateEphemerisDataAsset(activationRequest);
        result.activationStatus = activationResult.status;
        if (activationResult.status == skygate::ephemeris::EphemerisDataActivationStatus::Canceled) {
            result.status = StagedUpdateActivationStatus::Canceled;
            addDiagnostics(result, activationResult.diagnostics);
            if (result.diagnostics.empty()) {
                addDiagnostic(result, QStringLiteral("Ephemeris staged update activation was canceled."));
            }
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, m_activeCacheSnapshot, result);
            }
            cleanupCanceledStagingRoot(request, m_activeCacheSnapshot, result);
            return result;
        }
        if (!activationResult.isSuccess()) {
            result.status = StagedUpdateActivationStatus::ActivationFailed;
            addDiagnostics(result, activationResult.diagnostics);
            if (result.diagnostics.empty()) {
                addDiagnostic(result, QStringLiteral("Ephemeris staged update asset activation failed."));
            }
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, m_activeCacheSnapshot, result);
            }
            return result;
        }

        activePaths.emplace_back(asset->id, activationResult.activePath);
        result.activatedAssetIds.push_back(stringToQString(asset->id));
        if (isCanceled()) {
            result.status = StagedUpdateActivationStatus::Canceled;
            addDiagnostic(result, QStringLiteral("Ephemeris staged update activation was canceled before metadata."));
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, m_activeCacheSnapshot, result);
            }
            cleanupCanceledStagingRoot(request, m_activeCacheSnapshot, result);
            return result;
        }
    }

    EphemerisDataCacheSnapshot newSnapshot =
        cacheSnapshotForActivatedProfile(*request.manifest, *profile, activePaths, revisionToken);
    newSnapshot = normalizedSnapshot(std::move(newSnapshot));
    if (m_settingsStore == nullptr || !m_settingsStore->saveEphemerisDataCache(newSnapshot)) {
        result.status = StagedUpdateActivationStatus::PersistenceFailed;
        addDiagnostic(result, QStringLiteral("Unable to persist activated ephemeris data metadata."));
        if (request.cleanupFailedActivationCache) {
            cleanupInactiveActivationRoot(revisionCacheRoot, m_activeCacheSnapshot, result);
        }
        return result;
    }

    result.cacheSnapshot = newSnapshot;
    result.status = StagedUpdateActivationStatus::Activated;
    applyCacheSnapshot(
        std::move(newSnapshot), ActiveSource::Installed, QStringLiteral("Ephemeris data: Installed data active"), true
    );
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
        m_activeDataSnapshot = std::make_shared<SkyActiveEphemerisDataSnapshot>(
            m_activeCacheSnapshot, m_activeSource == ActiveSource::Installed
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
