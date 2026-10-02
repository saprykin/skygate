#include "SkyEphemerisSnapshotAdapter.hpp"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisDataLog, "skygate.ephemeris.data")

using EphemerisDataCacheSnapshot = SkySettingsStore::EphemerisDataCacheSnapshot;
using skygate::ephemeris::EphemerisDataManifest;
using skygate::ephemeris::EphemerisKernelDataAsset;
using skygate::ephemeris::EphemerisTextDataAsset;

std::filesystem::path pathFromQString(const QString& path)
{
    return std::filesystem::path(path.toStdString());
}

QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

const EphemerisDataManifest::Profile*
bundledFallbackProfile(const EphemerisDataManifest* manifest, const QString& profileId)
{
    if (manifest == nullptr) {
        return nullptr;
    }

    const std::string normalizedProfileId = profileId.trimmed().toStdString();
    if (!normalizedProfileId.empty()) {
        const EphemerisDataManifest::Profile* profile = manifest->profile(normalizedProfileId);
        return profile != nullptr && profile->bundled ? profile : nullptr;
    }

    const auto bundledShortRange =
        std::ranges::find_if(manifest->profiles, [](const EphemerisDataManifest::Profile& profile) {
            return profile.bundled && !profile.longRange;
        });
    if (bundledShortRange != manifest->profiles.end()) {
        return &*bundledShortRange;
    }

    return nullptr;
}

std::optional<EphemerisTextDataAsset>
loadTextAsset(const QString& path, const QString& id, const QString& version, const QString& provenance)
{
    if (path.isEmpty()) {
        return std::nullopt;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(skygateEphemerisDataLog).noquote()
            << "Unable to open installed ephemeris text data asset" << path << file.errorString();
        return std::nullopt;
    }

    EphemerisTextDataAsset asset;
    asset.id = id.toStdString();
    asset.version = version.toStdString();
    asset.provenance = provenance.toStdString();
    asset.content = QString::fromUtf8(file.readAll()).toStdString();
    return asset;
}

}  // namespace

SkyEphemerisSnapshotAdapter::SkyEphemerisSnapshotAdapter(
    EphemerisDataCacheSnapshot cacheSnapshot,
    const bool installedDataActive,
    const EphemerisDataManifest* bundledFallbackManifest,
    QString bundledFallbackResourceRoot,
    QString bundledFallbackProfileId
)
    : m_cacheSnapshot(std::move(cacheSnapshot)), m_bundledFallbackManifest(bundledFallbackManifest),
      m_bundledFallbackResourceRoot(std::move(bundledFallbackResourceRoot)),
      m_bundledFallbackProfileId(std::move(bundledFallbackProfileId)), m_installedDataActive(installedDataActive)
{
}

std::optional<EphemerisTextDataAsset> SkyEphemerisSnapshotAdapter::leapSecondTableAsset() const
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

std::optional<EphemerisTextDataAsset> SkyEphemerisSnapshotAdapter::deltaTDataAsset() const
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

std::optional<EphemerisTextDataAsset> SkyEphemerisSnapshotAdapter::earthOrientationDataAsset() const
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

std::optional<EphemerisKernelDataAsset>
SkyEphemerisSnapshotAdapter::solarSystemKernelAsset(const std::string_view assetId) const
{
    const QString requestedAssetId = QString::fromUtf8(assetId.data(), static_cast<qsizetype>(assetId.size()));
    if (m_installedDataActive && !m_cacheSnapshot.installedKernelPath.isEmpty()) {
        if (m_cacheSnapshot.installedKernelAssetId.isEmpty()
            || m_cacheSnapshot.installedKernelAssetId != requestedAssetId) {
            return std::nullopt;
        }

        EphemerisKernelDataAsset asset;
        asset.id = m_cacheSnapshot.installedKernelAssetId.toStdString();
        asset.profileId = m_cacheSnapshot.installedKernelProfileId.toStdString();
        asset.version = m_cacheSnapshot.installedKernelVersion.toStdString();
        asset.provenance = "Installed ephemeris data cache";
        asset.activePath = m_cacheSnapshot.installedKernelPath.toStdString();
        return asset;
    }

    const EphemerisDataManifest::Profile* profile =
        bundledFallbackProfile(m_bundledFallbackManifest, m_bundledFallbackProfileId);
    if (profile == nullptr || m_bundledFallbackResourceRoot.trimmed().isEmpty()) {
        return std::nullopt;
    }

    for (const std::string& profileAssetId : profile->assetIds) {
        if (profileAssetId != assetId) {
            continue;
        }
        const EphemerisDataManifest::Asset* manifestAsset = m_bundledFallbackManifest->asset(profileAssetId);
        if (manifestAsset == nullptr || manifestAsset->kind != EphemerisDataManifest::AssetKind::SolarSystemKernel
            || manifestAsset->relativePath.empty()) {
            return std::nullopt;
        }

        const std::filesystem::path activePath =
            pathFromQString(m_bundledFallbackResourceRoot) / std::filesystem::path(manifestAsset->relativePath);
        const QFileInfo activeFileInfo(pathToQString(activePath));
        if (!activeFileInfo.exists() || !activeFileInfo.isFile()) {
            return std::nullopt;
        }

        return EphemerisKernelDataAsset{
            .id = manifestAsset->id,
            .profileId = profile->id,
            .version = manifestAsset->version,
            .provenance = m_bundledFallbackManifest->dataSetInfo.provenance.empty()
                              ? "Bundled ephemeris fallback"
                              : m_bundledFallbackManifest->dataSetInfo.provenance,
            .activePath = pathToQString(activePath).toStdString(),
        };
    }

    return std::nullopt;
}
