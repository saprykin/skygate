#include "SkyEphemerisUpdateWorkflow.hpp"
#include "SkyEphemerisAssetStatus.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyEphemerisManifestLoader.hpp"
#include "SkyEphemerisPathSegmentPolicy.hpp"
#include "SkyEphemerisSettingsPolicy.hpp"
#include "SkyEphemerisStatusText.hpp"
#include "SkyEphemerisUpdateOperation.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QPointer>
#include <QStringList>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace skygate::ui::internal;

namespace {

using skygate::ephemeris::EphemerisDataManifest;
using skygate::ui::skygateEphemerisUpdateLog;
using skygate::ui::internal::SkyEphemerisAssetStatus;
using skygate::ui::internal::SkyEphemerisManifestLoader;
using skygate::ui::internal::SkyEphemerisUpdateOperation;

[[nodiscard]] QString sentenceFragment(QString text)
{
    text = text.trimmed();
    if (!text.isEmpty() && text.front().isUpper() && (text.size() == 1 || !text.at(1).isUpper())) {
        text.front() = text.front().toLower();
    }
    return text;
}

[[nodiscard]] QString diagnosticText(const QString& diagnostic)
{
    return diagnostic;
}

[[nodiscard]] QString diagnosticText(const std::string& diagnostic)
{
    return QString::fromStdString(diagnostic);
}

template <typename Diagnostics>
[[nodiscard]] QString joinedDiagnostics(const Diagnostics& diagnostics, const QString& fallback)
{
    QStringList parts;
    for (const auto& diagnostic : diagnostics) {
        const QString trimmed = diagnosticText(diagnostic).trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }

    return parts.isEmpty() ? fallback : parts.join(QStringLiteral("; "));
}

struct SupportDataStatus {
    QString text;
    bool updateAvailable = false;
};

[[nodiscard]] SupportDataStatus
supportDataStatusText(const SkyEphemerisAssetStatus& status, const QString& availableVersion, const bool updateChecked)
{
    const QString displayStatus = status.displayText();
    if (!updateChecked) {
        return {displayStatus, false};
    }
    if (!status.isInstalled()) {
        if (availableVersion.isEmpty()) {
            return {QStringLiteral("Bundled (latest)"), false};
        }
        return {QStringLiteral("Bundled (available: %1)").arg(availableVersion), true};
    }
    if (!availableVersion.isEmpty() && availableVersion > status.version()) {
        return {QStringLiteral("%1 (available: %2)").arg(displayStatus, availableVersion), true};
    }
    return {QStringLiteral("%1 (latest)").arg(displayStatus), false};
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
          writableCacheRoot.trimmed().isEmpty() ? SkyEphemerisSettingsPolicy::defaultWritableCacheRoot()
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

SkyEphemerisAssetStatus SkyEphemerisUpdateWorkflow::earthOrientationAssetStatus() const
{
    return SkyEphemerisAssetStatus::fromStatusText(
        m_dataManager != nullptr ? m_dataManager->earthOrientationStatusText() : QString()
    );
}

SkyEphemerisAssetStatus SkyEphemerisUpdateWorkflow::leapSecondAssetStatus() const
{
    return SkyEphemerisAssetStatus::fromStatusText(
        m_dataManager != nullptr ? m_dataManager->leapSecondStatusText() : QString()
    );
}

SkyEphemerisAssetStatus SkyEphemerisUpdateWorkflow::deltaTAssetStatus() const
{
    return SkyEphemerisAssetStatus::fromStatusText(
        m_dataManager != nullptr ? m_dataManager->deltaTStatusText() : QString()
    );
}

QString SkyEphemerisUpdateWorkflow::earthOrientationStatusText() const
{
    return supportDataStatusText(
               earthOrientationAssetStatus(), m_availableEarthOrientationVersion, m_supportDataUpdateChecked
    )
        .text;
}

QString SkyEphemerisUpdateWorkflow::leapSecondStatusText() const
{
    return supportDataStatusText(leapSecondAssetStatus(), m_availableLeapSecondVersion, m_supportDataUpdateChecked)
        .text;
}

QString SkyEphemerisUpdateWorkflow::deltaTStatusText() const
{
    return supportDataStatusText(deltaTAssetStatus(), m_availableDeltaTVersion, m_supportDataUpdateChecked).text;
}

bool SkyEphemerisUpdateWorkflow::updateCancellationRequested() const noexcept
{
    return m_dataManager != nullptr && m_dataManager->updateCancellationRequested();
}

bool SkyEphemerisUpdateWorkflow::isCurrentOperation(
    const std::shared_ptr<SkyEphemerisUpdateOperation>& operation
) const noexcept
{
    return operation != nullptr && operation == m_operation;
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
                setStatusText(SkyEphemerisStatusText::failure(diagnostic));
                completionHandler(false);
                return;
            }

            SkyEphemerisManifestLoader::Result loadResult = SkyEphemerisManifestLoader::load(downloadResult.stagedPath);
            if (loadResult.status == SkyEphemerisManifestLoader::Status::Missing
                || loadResult.status == SkyEphemerisManifestLoader::Status::OpenFailed) {
                qCWarning(skygateEphemerisUpdateLog).noquote() << "Unable to open staged ephemeris update manifest"
                                                               << downloadResult.stagedPath << loadResult.fileErrorText;
                setStatusText(SkyEphemerisStatusText::updateManifestUnreadable());
                completionHandler(false);
                return;
            }
            if (loadResult.status == SkyEphemerisManifestLoader::Status::TooLarge) {
                qCWarning(skygateEphemerisUpdateLog).noquote()
                    << "Ephemeris update manifest exceeds the allowed size" << loadResult.byteCount;
                setStatusText(SkyEphemerisStatusText::updateManifestTooLarge());
                completionHandler(false);
                return;
            }
            if (!loadResult.isSuccess()) {
                const QString diagnostic =
                    joinedDiagnostics(loadResult.diagnostics, QStringLiteral("manifest parse failed"));
                qCWarning(skygateEphemerisUpdateLog).noquote()
                    << "Unable to parse ephemeris update manifest:" << diagnostic;
                setStatusText(SkyEphemerisStatusText::failure(diagnostic));
                completionHandler(false);
                return;
            }

            m_refreshedManifest = std::move(loadResult.manifest);
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

bool SkyEphemerisUpdateWorkflow::ensureUpdateReady()
{
    if (m_updateInProgress) {
        setStatusText(SkyEphemerisStatusText::updateAlreadyInProgress());
        return false;
    }
    if (activeManifest() == nullptr) {
        setStatusText(SkyEphemerisStatusText::updateManifestUnavailable());
        return false;
    }
    if (m_writableCacheRoot.trimmed().isEmpty()) {
        setStatusText(SkyEphemerisStatusText::writableCacheUnavailable());
        return false;
    }
    return true;
}

bool SkyEphemerisUpdateWorkflow::prepareStagingRoot(const QString& stagedRoot, const QString& stagingLabel)
{
    QDir stagingDirectory(stagedRoot);
    if (stagingDirectory.exists() && !stagingDirectory.removeRecursively()) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to clear ephemeris" << stagingLabel << "staging directory" << stagedRoot;
        setStatusText(SkyEphemerisStatusText::stagingAreaUnclearable());
        return false;
    }
    if (!QDir().mkpath(stagedRoot)) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to create ephemeris" << stagingLabel << "staging directory" << stagedRoot;
        setStatusText(SkyEphemerisStatusText::stagingAreaUncreatable());
        return false;
    }
    return true;
}

bool SkyEphemerisUpdateWorkflow::checkSupportDataUpdates(const QString& manifestUrl)
{
    if (!ensureUpdateReady()) {
        return false;
    }

    const QString stagedRoot = m_writableCacheRoot + QStringLiteral("/staging/support-data-info");
    if (!prepareStagingRoot(stagedRoot, QStringLiteral("support-data"))) {
        return false;
    }

    if (m_dataManager != nullptr) {
        m_dataManager->clearUpdateCancellation();
    }
    m_updateInProgress = true;
    setUpdateProgress(0.0);
    setStatusText(SkyEphemerisStatusText::checkingSupportData());
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
            setStatusText(SkyEphemerisStatusText::updateManifestUnavailable());
            emit statusChanged();
            return;
        }

        m_availableEarthOrientationVersion =
            supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::EarthOrientationData);
        m_availableLeapSecondVersion = supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::LeapSecondTable);
        m_availableDeltaTVersion = supportDataVersion(*manifest, EphemerisDataManifest::AssetKind::DeltaTData);
        m_supportDataUpdateChecked = true;

        const SupportDataStatus earthOrientationStatus = supportDataStatusText(
            earthOrientationAssetStatus(), m_availableEarthOrientationVersion, m_supportDataUpdateChecked
        );
        const SupportDataStatus leapSecondStatus =
            supportDataStatusText(leapSecondAssetStatus(), m_availableLeapSecondVersion, m_supportDataUpdateChecked);
        const SupportDataStatus deltaTStatus =
            supportDataStatusText(deltaTAssetStatus(), m_availableDeltaTVersion, m_supportDataUpdateChecked);
        const bool updatesAvailable =
            earthOrientationStatus.updateAvailable || leapSecondStatus.updateAvailable || deltaTStatus.updateAvailable;
        setStatusText(
            updatesAvailable ? SkyEphemerisStatusText::supportDataUpdatesAvailable()
                             : SkyEphemerisStatusText::supportDataUpToDate()
        );
        emit statusChanged();
    });
    return true;
}

void SkyEphemerisUpdateWorkflow::cancelActiveUpdate()
{
    if (m_updateInProgress && m_dataManager != nullptr) {
        m_dataManager->requestUpdateCancellation();
        setStatusText(SkyEphemerisStatusText::cancelingDownload());
    }
}

bool SkyEphemerisUpdateWorkflow::updateProfile(const QString& profileId, const QString& manifestUrl)
{
    if (m_dataManager == nullptr) {
        setStatusText(SkyEphemerisStatusText::updateManagerUnavailable());
        return false;
    }
    if (!ensureUpdateReady()) {
        return false;
    }

    const QString normalizedProfileId = profileId.trimmed();
    if (normalizedProfileId.isEmpty()) {
        setStatusText(SkyEphemerisStatusText::updateProfileUnavailable());
        return false;
    }

    const QString stagedRoot =
        m_writableCacheRoot + QStringLiteral("/staging/")
        + SkyEphemerisPathSegmentPolicy::spellPathSegment(normalizedProfileId, QStringLiteral("profile"));
    if (!prepareStagingRoot(stagedRoot, QStringLiteral("profile"))) {
        return false;
    }

    m_updateInProgress = true;
    if (m_dataManager != nullptr) {
        m_dataManager->clearUpdateCancellation();
    }
    setUpdateProgress(0.0);
    emit statusChanged();
    setStatusText(SkyEphemerisStatusText::checkingUpdateManifest());
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
    m_operation.reset();
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
        finishUpdate(false, SkyEphemerisStatusText::updateManifestUnavailable());
        return;
    }

    const skygate::ephemeris::EphemerisDataManifest::Profile* profile =
        updateManifest->profile(profileId.toStdString());
    if (profile == nullptr) {
        finishUpdate(false, SkyEphemerisStatusText::updateProfileUnavailable());
        return;
    }

    const QString progressName = profileProgressName(*updateManifest, *profile);
    const auto operation = std::make_shared<SkyEphemerisUpdateOperation>(
        *updateManifest, *profile, profileId, progressName, stagedRoot, m_writableCacheRoot
    );
    m_operation = operation;

    setStatusText(SkyEphemerisStatusText::downloading(progressName));
    dispatchNextAsset();
}

void SkyEphemerisUpdateWorkflow::dispatchNextAsset()
{
    if (m_operation == nullptr) {
        return;
    }

    const std::shared_ptr<SkyEphemerisUpdateOperation> operation = m_operation;
    if (!operation->hasPendingAsset()) {
        activateOperation(operation);
        return;
    }

    const skygate::ephemeris::EphemerisDataManifest::Asset* asset = operation->currentAsset();
    if (asset == nullptr) {
        finishUpdate(false, SkyEphemerisStatusText::updateProfileMissingAsset());
        return;
    }

    SkyEphemerisDataManager::StagedUpdateDownloadRequest downloadRequest;
    downloadRequest.asset = asset;
    downloadRequest.sourceUrl = QString::fromStdString(asset->sourceUrl);
    if (downloadRequest.sourceUrl.trimmed().isEmpty()) {
        downloadRequest.sourceResourceRoot = m_updateResourceRoot;
    }
    downloadRequest.stagedResourceRoot = operation->stagedRoot();

    const QPointer<SkyEphemerisUpdateWorkflow> workflow(this);
    const std::weak_ptr<SkyEphemerisUpdateOperation> operationWeak = operation;
    downloadRequest.cancellationRequested = [workflow] {
        return workflow.isNull() || workflow->updateCancellationRequested();
    };
    downloadRequest.progressHandler =
        [workflow, operationWeak](const std::uint64_t stagedBytes, const std::optional<std::uint64_t> totalBytes) {
            if (workflow.isNull()) {
                return;
            }
            const auto currentOperation = operationWeak.lock();
            if (currentOperation != nullptr) {
                workflow->reportAssetProgress(currentOperation, stagedBytes, totalBytes);
            }
        };

    m_dataManager->stageEphemerisUpdateAssetAsync(
        downloadRequest, [workflow, operationWeak](SkyEphemerisDataManager::StagedUpdateDownloadResult downloadResult) {
            if (workflow.isNull()) {
                return;
            }
            const auto currentOperation = operationWeak.lock();
            if (currentOperation == nullptr) {
                return;
            }
            if (!downloadResult.isSuccess()) {
                workflow->failAssetDownload(
                    currentOperation,
                    joinedDiagnostics(downloadResult.diagnostics, QStringLiteral("asset download failed"))
                );
                return;
            }
            workflow->completeAssetDownload(currentOperation, downloadResult.stagedBytes);
        }
    );
}

void SkyEphemerisUpdateWorkflow::reportAssetProgress(
    const std::shared_ptr<SkyEphemerisUpdateOperation>& operation,
    const std::uint64_t stagedBytes,
    const std::optional<std::uint64_t> totalBytes
)
{
    if (!isCurrentOperation(operation)) {
        return;
    }
    if (const std::optional<double> progress = operation->stagedProgress(stagedBytes, totalBytes)) {
        setUpdateProgress(*progress);
    }
}

void SkyEphemerisUpdateWorkflow::completeAssetDownload(
    const std::shared_ptr<SkyEphemerisUpdateOperation>& operation, const std::uint64_t stagedBytes
)
{
    if (!isCurrentOperation(operation)) {
        return;
    }

    const skygate::ephemeris::EphemerisDataManifest::Asset* asset = operation->currentAsset();
    if (asset == nullptr) {
        finishUpdate(false, SkyEphemerisStatusText::updateProfileMissingAsset());
        return;
    }

    operation->recordStagedAsset(*asset, stagedBytes);
    if (operation->expectedBytes() > 0U) {
        setUpdateProgress(
            static_cast<double>(std::min(operation->completedBytes(), operation->expectedBytes()))
            / static_cast<double>(operation->expectedBytes())
        );
    }

    operation->advanceAsset();
    dispatchNextAsset();
}

void SkyEphemerisUpdateWorkflow::failAssetDownload(
    const std::shared_ptr<SkyEphemerisUpdateOperation>& operation, QString diagnostic
)
{
    if (!isCurrentOperation(operation)) {
        return;
    }
    finishUpdate(false, SkyEphemerisStatusText::failure(diagnostic));
}

void SkyEphemerisUpdateWorkflow::activateOperation(const std::shared_ptr<SkyEphemerisUpdateOperation>& operation)
{
    if (!isCurrentOperation(operation)) {
        return;
    }

    setStatusText(SkyEphemerisStatusText::verifying(operation->progressName()));
    const SkyEphemerisDataManager::StagedUpdateActivationResult activationResult =
        m_dataManager->activateVerifiedStagedUpdateSet(operation->activationRequest());
    if (!isCurrentOperation(operation)) {
        return;
    }

    if (activationResult.isSuccess()) {
        emit preferredProfileSelected(operation->profileId());
        if (operation->profileId() == QStringLiteral("support-data")) {
            m_supportDataUpdateChecked = false;
            m_availableEarthOrientationVersion.clear();
            m_availableLeapSecondVersion.clear();
            m_availableDeltaTVersion.clear();
        }
        finishUpdate(true, {});
        return;
    }

    const QString diagnostic = joinedDiagnostics(activationResult.diagnostics, QStringLiteral("activation failed"));
    finishUpdate(false, SkyEphemerisStatusText::failure(diagnostic));
}
