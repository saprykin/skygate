#include "SkyEphemerisUpdateWorkflow.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyEphemerisPathSegmentPolicy.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QPointer>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisUpdateLog, "skygate.ephemeris.update")

constexpr std::uint64_t kMaxEphemerisManifestBytes = 4ULL * 1024ULL * 1024ULL;

using skygate::ephemeris::EphemerisDataManifest;

[[nodiscard]] QString sentenceFragment(QString text)
{
    text = text.trimmed();
    if (!text.isEmpty() && text.front().isUpper() && (text.size() == 1 || !text.at(1).isUpper())) {
        text.front() = text.front().toLower();
    }
    return text;
}

[[nodiscard]] QString displayDataStatus(QString statusText)
{
    statusText = statusText.trimmed();
    constexpr QLatin1StringView kInstalledPrefix("Installed: ");
    if (statusText.startsWith(kInstalledPrefix)) {
        return statusText.mid(kInstalledPrefix.size());
    }
    if (statusText.startsWith(QStringLiteral("Bundled fallback"))) {
        return QStringLiteral("Bundled");
    }
    return statusText;
}

[[nodiscard]] QString joinedDiagnostics(const std::vector<QString>& diagnostics, const QString& fallback)
{
    QStringList parts;
    for (const QString& diagnostic : diagnostics) {
        const QString trimmed = diagnostic.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }

    return parts.isEmpty() ? fallback : parts.join(QStringLiteral("; "));
}

[[nodiscard]] QString joinedDiagnostics(const std::vector<std::string>& diagnostics, const QString& fallback)
{
    QStringList parts;
    for (const std::string& diagnostic : diagnostics) {
        const QString trimmed = QString::fromStdString(diagnostic).trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }

    return parts.isEmpty() ? fallback : parts.join(QStringLiteral("; "));
}

[[nodiscard]] QString
supportDataStatusText(const QString& rawStatusText, const QString& availableVersion, const bool updateChecked)
{
    const QString displayStatus = displayDataStatus(rawStatusText);
    if (!updateChecked) {
        return displayStatus;
    }
    if (displayStatus == QStringLiteral("Bundled")) {
        return availableVersion.isEmpty() ? QStringLiteral("Bundled (latest)")
                                          : QStringLiteral("Bundled (available: %1)").arg(availableVersion);
    }
    if (!availableVersion.isEmpty() && availableVersion > displayStatus) {
        return QStringLiteral("%1 (available: %2)").arg(displayStatus, availableVersion);
    }
    return QStringLiteral("%1 (latest)").arg(displayStatus);
}

[[nodiscard]] QString
supportDataVersion(const EphemerisDataManifest& manifest, const EphemerisDataManifest::AssetKind kind)
{
    const EphemerisDataManifest::Profile* profile = manifest.profile("support-data");
    if (profile == nullptr) {
        return {};
    }
    for (const std::string& assetId : profile->assetIds) {
        const EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
        if (asset != nullptr && asset->kind == kind) {
            return QString::fromStdString(asset->version).trimmed();
        }
    }
    return {};
}

[[nodiscard]] QString
profileProgressName(const EphemerisDataManifest& manifest, const EphemerisDataManifest::Profile& profile)
{
    for (const std::string& assetId : profile.assetIds) {
        const EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
        if (asset != nullptr && asset->kind == EphemerisDataManifest::AssetKind::SolarSystemKernel) {
            return QString::fromStdString(asset->version).trimmed();
        }
    }
    return sentenceFragment(
        profile.displayName.empty() ? QString::fromStdString(profile.id) : QString::fromStdString(profile.displayName)
    );
}

}  // namespace

SkyEphemerisUpdateWorkflow::SkyEphemerisUpdateWorkflow(
    SkyEphemerisDataManager* dataManager,
    const skygate::ephemeris::EphemerisDataManifest* bundledFallbackManifest,
    QString updateResourceRoot,
    QString writableCacheRoot,
    QObject* parent
)
    : QObject(parent), m_dataManager(dataManager), m_bundledFallbackManifest(bundledFallbackManifest),
      m_updateResourceRoot(std::move(updateResourceRoot)),
      m_writableCacheRoot(
          writableCacheRoot.trimmed().isEmpty()
              ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/ephemeris-data")
              : writableCacheRoot.trimmed()
      )
{
}

SkyEphemerisUpdateWorkflow::~SkyEphemerisUpdateWorkflow() = default;

const skygate::ephemeris::EphemerisDataManifest* SkyEphemerisUpdateWorkflow::activeManifest() const noexcept
{
    return m_refreshedManifest.has_value() ? &*m_refreshedManifest : m_bundledFallbackManifest;
}

QString SkyEphemerisUpdateWorkflow::statusText() const
{
    if (!m_statusText.isEmpty()) {
        return m_statusText;
    }
    return m_dataManager != nullptr ? m_dataManager->statusText() : QString();
}

QString SkyEphemerisUpdateWorkflow::earthOrientationStatusText() const
{
    return supportDataStatusText(
        m_dataManager != nullptr ? m_dataManager->earthOrientationStatusText() : QString(),
        m_availableEarthOrientationVersion,
        m_supportDataUpdateChecked
    );
}

QString SkyEphemerisUpdateWorkflow::leapSecondStatusText() const
{
    return supportDataStatusText(
        m_dataManager != nullptr ? m_dataManager->leapSecondStatusText() : QString(),
        m_availableLeapSecondVersion,
        m_supportDataUpdateChecked
    );
}

QString SkyEphemerisUpdateWorkflow::deltaTStatusText() const
{
    return supportDataStatusText(
        m_dataManager != nullptr ? m_dataManager->deltaTStatusText() : QString(),
        m_availableDeltaTVersion,
        m_supportDataUpdateChecked
    );
}

bool SkyEphemerisUpdateWorkflow::updateCancellationRequested() const noexcept
{
    return m_dataManager != nullptr && m_dataManager->updateCancellationRequested();
}

bool SkyEphemerisUpdateWorkflow::updateEnabled() const noexcept
{
    return !m_updateInProgress && activeManifest() != nullptr && !m_updateResourceRoot.trimmed().isEmpty()
           && !m_writableCacheRoot.trimmed().isEmpty();
}

bool SkyEphemerisUpdateWorkflow::isUpdateInProgress() const noexcept
{
    return m_updateInProgress;
}

double SkyEphemerisUpdateWorkflow::updateProgress() const noexcept
{
    return m_updateProgress;
}

void SkyEphemerisUpdateWorkflow::setStatusText(QString statusText)
{
    statusText = statusText.trimmed();
    if (m_statusText == statusText) {
        return;
    }

    m_statusText = std::move(statusText);
    emit statusChanged();
}

void SkyEphemerisUpdateWorkflow::setUpdateProgress(const double progress) noexcept
{
    const double boundedProgress = std::clamp(progress, 0.0, 1.0);
    if (m_updateProgress == boundedProgress) {
        return;
    }

    m_updateProgress = boundedProgress;
    emit statusChanged();
}

void SkyEphemerisUpdateWorkflow::refreshManifestAsync(
    const QString& manifestUrl, const QString& stagedRoot, std::function<void(bool)> completionHandler
)
{
    if (manifestUrl.isEmpty()) {
        completionHandler(activeManifest() != nullptr);
        return;
    }
    if (m_dataManager == nullptr) {
        completionHandler(false);
        return;
    }

    auto manifestAsset = std::make_shared<EphemerisDataManifest::Asset>();
    manifestAsset->id = "manifest";
    manifestAsset->kind = EphemerisDataManifest::AssetKind::DeltaTData;
    manifestAsset->profileId = "manifest";
    manifestAsset->version = "manifest";
    manifestAsset->sourceUrl = manifestUrl.toStdString();
    manifestAsset->relativePath = "manifest.json";

    SkyEphemerisDataManager::StagedUpdateDownloadRequest request;
    request.asset = manifestAsset.get();
    request.sourceUrl = manifestUrl;
    request.stagedResourceRoot = stagedRoot + QStringLiteral("/manifest");
    const QPointer<SkyEphemerisUpdateWorkflow> workflow(this);
    request.cancellationRequested = [workflow] { return workflow.isNull() || workflow->updateCancellationRequested(); };

    m_dataManager->stageEphemerisUpdateAssetAsync(
        request,
        [this, workflow, manifestAsset, completionHandler](
            SkyEphemerisDataManager::StagedUpdateDownloadResult downloadResult
        ) {
            if (workflow.isNull()) {
                return;
            }
            if (!downloadResult.isSuccess()) {
                const QString diagnostic =
                    joinedDiagnostics(downloadResult.diagnostics, QStringLiteral("manifest download failed"));
                setStatusText(QStringLiteral("Ephemeris data: %1").arg(diagnostic));
                completionHandler(false);
                return;
            }

            QFile manifestFile(downloadResult.stagedPath);
            if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qCWarning(skygateEphemerisUpdateLog).noquote()
                    << "Unable to open staged ephemeris update manifest" << downloadResult.stagedPath
                    << manifestFile.errorString();
                setStatusText(QStringLiteral("Ephemeris data: Unable to open update manifest"));
                completionHandler(false);
                return;
            }

            if (manifestFile.size() > static_cast<qint64>(kMaxEphemerisManifestBytes)) {
                qCWarning(skygateEphemerisUpdateLog).noquote()
                    << "Ephemeris update manifest exceeds the allowed size" << manifestFile.size();
                setStatusText(QStringLiteral("Ephemeris data: Update manifest is too large"));
                completionHandler(false);
                return;
            }

            const QByteArray payload = manifestFile.readAll();
            EphemerisDataManifest::ParseResult parseResult = EphemerisDataManifest::parse(
                std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()))
            );
            if (!parseResult.isSuccess()) {
                const QString diagnostic =
                    joinedDiagnostics(parseResult.diagnostics, QStringLiteral("manifest parse failed"));
                qCWarning(skygateEphemerisUpdateLog).noquote()
                    << "Unable to parse ephemeris update manifest:" << diagnostic;
                setStatusText(QStringLiteral("Ephemeris data: %1").arg(diagnostic));
                completionHandler(false);
                return;
            }

            m_refreshedManifest = std::move(parseResult.manifest);
            completionHandler(true);
        }
    );
}

bool SkyEphemerisUpdateWorkflow::clearInstalledDataCache()
{
    const bool cleared = m_dataManager != nullptr && m_dataManager->clearInstalledDataCache();
    if (cleared) {
        setStatusText({});
    }
    return cleared;
}

bool SkyEphemerisUpdateWorkflow::clearPlanetaryKernelCache()
{
    const bool cleared = m_dataManager != nullptr && m_dataManager->clearPlanetaryKernelCache();
    if (cleared) {
        setStatusText({});
        emit statusChanged();
    }
    return cleared;
}

bool SkyEphemerisUpdateWorkflow::clearSupportDataCache()
{
    const bool cleared = m_dataManager != nullptr && m_dataManager->clearSupportDataCache();
    if (cleared) {
        m_supportDataUpdateChecked = false;
        m_availableEarthOrientationVersion.clear();
        m_availableLeapSecondVersion.clear();
        m_availableDeltaTVersion.clear();
        setStatusText({});
        emit statusChanged();
    }
    return cleared;
}

bool SkyEphemerisUpdateWorkflow::checkSupportDataUpdates(const QString& manifestUrl)
{
    if (m_updateInProgress) {
        setStatusText(QStringLiteral("Ephemeris data: Update already in progress"));
        return false;
    }
    if (activeManifest() == nullptr) {
        setStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
        return false;
    }
    if (m_writableCacheRoot.trimmed().isEmpty()) {
        setStatusText(QStringLiteral("Ephemeris data: Writable cache unavailable"));
        return false;
    }

    const QString stagedRoot = m_writableCacheRoot + QStringLiteral("/staging/support-data-info");
    QDir stagingDirectory(stagedRoot);
    if (stagingDirectory.exists() && !stagingDirectory.removeRecursively()) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to clear ephemeris support-data staging directory" << stagedRoot;
        setStatusText(QStringLiteral("Ephemeris data: Unable to clear update staging area"));
        return false;
    }
    if (!QDir().mkpath(stagedRoot)) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to create ephemeris support-data staging directory" << stagedRoot;
        setStatusText(QStringLiteral("Ephemeris data: Unable to create update staging area"));
        return false;
    }

    if (m_dataManager != nullptr) {
        m_dataManager->clearUpdateCancellation();
    }
    m_updateInProgress = true;
    setUpdateProgress(0.0);
    setStatusText(QStringLiteral("Ephemeris data: Checking time and Earth data..."));
    refreshManifestAsync(manifestUrl, stagedRoot, [this](const bool refreshed) {
        m_updateInProgress = false;
        if (m_dataManager != nullptr) {
            m_dataManager->clearUpdateCancellation();
        }
        setUpdateProgress(refreshed ? 1.0 : 0.0);
        if (!refreshed) {
            emit statusChanged();
            return;
        }

        const skygate::ephemeris::EphemerisDataManifest* manifest = activeManifest();
        if (manifest == nullptr) {
            setStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
            emit statusChanged();
            return;
        }

        m_availableEarthOrientationVersion =
            supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::EarthOrientationData);
        m_availableLeapSecondVersion = supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::LeapSecondTable);
        m_availableDeltaTVersion = supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::DeltaTData);
        m_supportDataUpdateChecked = true;
        setStatusText({});
        emit statusChanged();
    });
    return true;
}

void SkyEphemerisUpdateWorkflow::cancelActiveUpdate()
{
    if (m_updateInProgress && m_dataManager != nullptr) {
        m_dataManager->requestUpdateCancellation();
        setStatusText(QStringLiteral("Ephemeris data: Canceling download..."));
    }
}

bool SkyEphemerisUpdateWorkflow::updateProfile(const QString& profileId, const QString& manifestUrl)
{
    if (m_dataManager == nullptr) {
        setStatusText(QStringLiteral("Ephemeris data: Update manager unavailable"));
        return false;
    }
    if (m_updateInProgress) {
        setStatusText(QStringLiteral("Ephemeris data: Update already in progress"));
        return false;
    }
    if (activeManifest() == nullptr) {
        setStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
        return false;
    }
    if (m_writableCacheRoot.trimmed().isEmpty()) {
        setStatusText(QStringLiteral("Ephemeris data: Writable cache unavailable"));
        return false;
    }

    const QString normalizedProfileId = profileId.trimmed();
    if (normalizedProfileId.isEmpty()) {
        setStatusText(QStringLiteral("Ephemeris data: Update profile unavailable"));
        return false;
    }

    const QString stagedRoot =
        m_writableCacheRoot + QStringLiteral("/staging/")
        + SkyEphemerisPathSegmentPolicy::spellPathSegment(normalizedProfileId, QStringLiteral("profile"));
    QDir stagingDirectory(stagedRoot);
    if (stagingDirectory.exists() && !stagingDirectory.removeRecursively()) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to clear ephemeris profile staging directory" << stagedRoot;
        setStatusText(QStringLiteral("Ephemeris data: Unable to clear update staging area"));
        return false;
    }
    if (!QDir().mkpath(stagedRoot)) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to create ephemeris profile staging directory" << stagedRoot;
        setStatusText(QStringLiteral("Ephemeris data: Unable to create update staging area"));
        return false;
    }

    m_updateInProgress = true;
    if (m_dataManager != nullptr) {
        m_dataManager->clearUpdateCancellation();
    }
    setUpdateProgress(0.0);
    emit statusChanged();
    setStatusText(QStringLiteral("Ephemeris data: Checking update manifest..."));
    refreshManifestAsync(manifestUrl, stagedRoot, [this, normalizedProfileId, stagedRoot](const bool refreshed) {
        if (!refreshed) {
            finishUpdate(false, m_statusText);
            return;
        }
        continueUpdate(normalizedProfileId, stagedRoot);
    });
    return true;
}

void SkyEphemerisUpdateWorkflow::finishUpdate(const bool success, QString statusText)
{
    if (!success && !statusText.trimmed().isEmpty()) {
        qCWarning(skygateEphemerisUpdateLog).noquote() << statusText;
    }
    m_updateInProgress = false;
    if (m_dataManager != nullptr) {
        m_dataManager->clearUpdateCancellation();
    }
    setUpdateProgress(success ? 1.0 : 0.0);
    setStatusText(std::move(statusText));
    emit statusChanged();
}

void SkyEphemerisUpdateWorkflow::continueUpdate(const QString& profileId, const QString& stagedRoot)
{
    const skygate::ephemeris::EphemerisDataManifest* updateManifest = activeManifest();
    if (updateManifest == nullptr) {
        finishUpdate(false, QStringLiteral("Ephemeris data: Update manifest unavailable"));
        return;
    }

    const skygate::ephemeris::EphemerisDataManifest::Profile* profile =
        updateManifest->profile(profileId.toStdString());
    if (profile == nullptr) {
        finishUpdate(false, QStringLiteral("Ephemeris data: Update profile unavailable"));
        return;
    }

    std::uint64_t totalExpectedBytes = 0U;
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifest::Asset* asset = updateManifest->asset(assetId);
        if (asset != nullptr && asset->compression.uncompressedSizeBytes.has_value()) {
            totalExpectedBytes += *asset->compression.uncompressedSizeBytes;
        }
    }

    const QString progressName = profileProgressName(*updateManifest, *profile);
    setStatusText(QStringLiteral("Ephemeris data: Downloading %1...").arg(progressName));

    auto request = std::make_shared<SkyEphemerisDataManager::StagedUpdateActivationRequest>();
    request->manifest = updateManifest;
    request->profileId = profileId;
    request->stagedResourceRoot = stagedRoot;
    request->writableCacheRoot = m_writableCacheRoot;
    request->revisionToken = QString::fromStdString(profile->id);

    const auto completedBytes = std::make_shared<std::uint64_t>(0U);
    const auto downloadNext = std::make_shared<std::function<void(std::size_t)>>();
    const auto downloadNextWeak = std::weak_ptr<std::function<void(std::size_t)>>(downloadNext);
    const QPointer<SkyEphemerisUpdateWorkflow> workflow(this);
    *downloadNext = [this,
                     workflow,
                     profileId,
                     updateManifest,
                     profile,
                     request,
                     totalExpectedBytes,
                     completedBytes,
                     progressName,
                     downloadNextWeak,
                     stagedRoot](const std::size_t assetIndex) {
        if (workflow.isNull()) {
            return;
        }
        const auto next = downloadNextWeak.lock();
        if (assetIndex >= profile->assetIds.size()) {
            setStatusText(QStringLiteral("Ephemeris data: Verifying %1...").arg(progressName));
            const SkyEphemerisDataManager::StagedUpdateActivationResult activationResult =
                m_dataManager->activateVerifiedStagedUpdateSet(*request);
            if (activationResult.isSuccess()) {
                emit preferredProfileSelected(profileId);
                if (profileId == QStringLiteral("support-data")) {
                    m_supportDataUpdateChecked = false;
                    m_availableEarthOrientationVersion.clear();
                    m_availableLeapSecondVersion.clear();
                    m_availableDeltaTVersion.clear();
                }
                finishUpdate(true, {});
            } else {
                const QString diagnostic =
                    joinedDiagnostics(activationResult.diagnostics, QStringLiteral("activation failed"));
                finishUpdate(false, QStringLiteral("Ephemeris data: %1").arg(diagnostic));
            }
            return;
        }

        const std::string assetId = profile->assetIds[assetIndex];
        const skygate::ephemeris::EphemerisDataManifest::Asset* asset = updateManifest->asset(assetId);
        if (asset == nullptr) {
            finishUpdate(false, QStringLiteral("Ephemeris data: Update profile references a missing asset"));
            return;
        }

        SkyEphemerisDataManager::StagedUpdateDownloadRequest downloadRequest;
        downloadRequest.asset = asset;
        downloadRequest.sourceUrl = QString::fromStdString(asset->sourceUrl);
        if (downloadRequest.sourceUrl.trimmed().isEmpty()) {
            downloadRequest.sourceResourceRoot = m_updateResourceRoot;
        }
        downloadRequest.stagedResourceRoot = stagedRoot;
        downloadRequest.cancellationRequested = [workflow] {
            return workflow.isNull() || workflow->updateCancellationRequested();
        };
        downloadRequest.progressHandler = [this, workflow, completedBytes, totalExpectedBytes](
                                              const std::uint64_t stagedBytes,
                                              const std::optional<std::uint64_t> totalBytes
                                          ) {
            if (workflow.isNull()) {
                return;
            }
            const std::uint64_t denominator =
                totalExpectedBytes > 0U ? totalExpectedBytes : *completedBytes + totalBytes.value_or(stagedBytes);
            if (denominator > 0U) {
                setUpdateProgress(
                    static_cast<double>(std::min(*completedBytes + stagedBytes, denominator))
                    / static_cast<double>(denominator)
                );
            }
        };

        m_dataManager->stageEphemerisUpdateAssetAsync(
            downloadRequest, [=, this](const SkyEphemerisDataManager::StagedUpdateDownloadResult downloadResult) {
                if (workflow.isNull()) {
                    return;
                }
                if (!downloadResult.isSuccess()) {
                    const QString diagnostic =
                        joinedDiagnostics(downloadResult.diagnostics, QStringLiteral("asset download failed"));
                    finishUpdate(false, QStringLiteral("Ephemeris data: %1").arg(diagnostic));
                    return;
                }
                *completedBytes += asset->compression.uncompressedSizeBytes.value_or(downloadResult.stagedBytes);
                if (totalExpectedBytes > 0U) {
                    setUpdateProgress(
                        static_cast<double>(std::min(*completedBytes, totalExpectedBytes))
                        / static_cast<double>(totalExpectedBytes)
                    );
                }
                request->requiredKinds.push_back(asset->kind);
                auto& component = request->expectedComponents.emplace_back(asset->id, asset->kind);
                component.expectedVersion = asset->version;
                component.requiredValidityRange = asset->validityRange;
                if (next != nullptr) {
                    (*next)(assetIndex + 1);
                } else {
                    finishUpdate(false, QStringLiteral("Ephemeris data: Update download flow ended unexpectedly"));
                }
            }
        );
    };
    (*downloadNext)(0);
}
