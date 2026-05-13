#include "SkyEphemerisDataManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using EphemerisDataCacheSnapshot = SkySettingsStore::EphemerisDataCacheSnapshot;
using skygate::ephemeris::EphemerisTextDataAsset;
using skygate::ephemeris::IEphemerisDataSnapshot;
using KernelDataAsset = skygate::ephemeris::EphemerisKernelDataAsset;

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
    snapshot.installedLeapSecondTableVersion = trimmed(snapshot.installedLeapSecondTableVersion);
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
           || !snapshot.installedLeapSecondTableVersion.isEmpty() || !snapshot.installedDeltaTDataVersion.isEmpty()
           || snapshot.dataRevisionToken != EphemerisDataCacheSnapshot{}.dataRevisionToken;
}

QStringList missingInstalledPaths(const EphemerisDataCacheSnapshot& snapshot)
{
    QStringList missingPaths;
    const QStringList candidatePaths{snapshot.installedKernelPath, snapshot.installedEarthOrientationPath};
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
           && lhs.installedLeapSecondTableVersion == rhs.installedLeapSecondTableVersion
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

class SkyActiveEphemerisDataSnapshot final : public IEphemerisDataSnapshot {
public:
    SkyActiveEphemerisDataSnapshot(EphemerisDataCacheSnapshot cacheSnapshot, const bool installedDataActive)
        : m_cacheSnapshot(std::move(cacheSnapshot)), m_installedDataActive(installedDataActive)
    {
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> deltaTDataAsset() const override
    {
        return std::nullopt;
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
