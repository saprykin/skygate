#include "SkyEphemerisCacheController.hpp"

#include "engine/EphemerisDataActivation.hpp"
#include "engine/EphemerisStagedUpdateVerification.hpp"

#include <QLoggingCategory>
#include <QStringList>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisDataLog, "skygate.ephemeris.data")

using EphemerisDataCacheSnapshot = SkySettingsStore::EphemerisDataCacheSnapshot;
using StagedUpdateActivationRequest = SkyEphemerisCacheController::StagedUpdateActivationRequest;
using StagedUpdateActivationResult = SkyEphemerisCacheController::StagedUpdateActivationResult;
using StagedUpdateActivationStatus = SkyEphemerisCacheController::StagedUpdateActivationStatus;
using skygate::ephemeris::EphemerisDataActivationRequest;
using skygate::ephemeris::EphemerisDataManifest;
using skygate::ephemeris::EphemerisStagedUpdateVerificationRequest;
using skygate::ephemeris::EphemerisStagedUpdateVerificationResult;
using skygate::ephemeris::EphemerisStagedUpdateVerificationStatus;

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

void addDiagnostic(StagedUpdateActivationResult& result, QString diagnostic)
{
    if (!diagnostic.trimmed().isEmpty()) {
        qCWarning(skygateEphemerisDataLog).noquote() << diagnostic;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

void addDiagnostics(StagedUpdateActivationResult& result, const std::vector<std::string>& diagnostics)
{
    for (const std::string& diagnostic : diagnostics) {
        addDiagnostic(result, stringToQString(diagnostic));
    }
}

bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(StagedUpdateActivationResult& result, QString diagnostic)
{
    result.status = StagedUpdateActivationStatus::Canceled;
    result.verificationStatus = EphemerisStagedUpdateVerificationStatus::Canceled;
    addDiagnostic(result, std::move(diagnostic));
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

QString defaultRevisionToken(const EphemerisDataManifest& manifest, const EphemerisDataManifest::Profile& profile)
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

bool cacheRootContainsActivePath(const std::filesystem::path& root, const EphemerisDataCacheSnapshot& activeSnapshot)
{
    return pathContains(root, activeSnapshot.installedKernelPath)
           || pathContains(root, activeSnapshot.installedEarthOrientationPath)
           || pathContains(root, activeSnapshot.installedLeapSecondTablePath)
           || pathContains(root, activeSnapshot.installedDeltaTDataPath);
}

void cleanupInactiveActivationRoot(
    const std::filesystem::path& activationRoot,
    const EphemerisDataCacheSnapshot& activeSnapshot,
    StagedUpdateActivationResult& result
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
    const StagedUpdateActivationRequest& request,
    const EphemerisDataCacheSnapshot& activeSnapshot,
    StagedUpdateActivationResult& result
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
    const EphemerisDataManifest::Profile& profile,
    const std::vector<std::pair<std::string, std::filesystem::path>>& activePaths,
    const EphemerisDataCacheSnapshot& baseSnapshot,
    const QString& revisionToken
)
{
    EphemerisDataCacheSnapshot snapshot = baseSnapshot;
    snapshot.dataRevisionToken = revisionToken;
    snapshot.lastUpdateResult =
        QStringLiteral("Installed %1")
            .arg(profile.displayName.empty() ? stringToQString(profile.id) : stringToQString(profile.displayName));

    for (const std::string& assetId : profile.assetIds) {
        const EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
        if (asset == nullptr) {
            continue;
        }
        const auto activePath = std::find_if(activePaths.begin(), activePaths.end(), [asset](const auto& entry) {
            return entry.first == asset->id;
        });

        switch (asset->kind) {
        case EphemerisDataManifest::AssetKind::SolarSystemKernel:
            snapshot.installedKernelAssetId = stringToQString(asset->id);
            snapshot.installedKernelProfileId = stringToQString(asset->profileId);
            snapshot.installedKernelVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedKernelPath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifest::AssetKind::EarthOrientationData:
            snapshot.installedEarthOrientationVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedEarthOrientationPath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifest::AssetKind::LeapSecondTable:
            snapshot.installedLeapSecondTableVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedLeapSecondTablePath = pathToQString(activePath->second);
            }
            break;
        case EphemerisDataManifest::AssetKind::DeltaTData:
            snapshot.installedDeltaTDataVersion = stringToQString(asset->version);
            if (activePath != activePaths.end()) {
                snapshot.installedDeltaTDataPath = pathToQString(activePath->second);
            }
            break;
        }
    }

    return snapshot;
}

}  // namespace

SkyEphemerisCacheController::StagedUpdateActivationResult SkyEphemerisCacheController::activate(
    const StagedUpdateActivationRequest& request,
    const EphemerisDataCacheSnapshot& activeSnapshot,
    SkySettingsStore* settingsStore
) const
{
    StagedUpdateActivationResult result;
    const auto isCanceled = [&request] { return cancellationRequested(request.cancellationRequested); };
    if (isCanceled()) {
        markCanceled(result, QStringLiteral("Ephemeris staged update activation was canceled before verification."));
        cleanupCanceledStagingRoot(request, activeSnapshot, result);
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
        skygate::ephemeris::EphemerisStagedUpdateVerification::verify(verificationRequest);
    result.verificationStatus = verificationResult.status;
    if (verificationResult.status == EphemerisStagedUpdateVerificationStatus::Canceled) {
        result.status = StagedUpdateActivationStatus::Canceled;
        addDiagnostics(result, verificationResult.diagnostics);
        if (result.diagnostics.empty()) {
            addDiagnostic(result, QStringLiteral("Ephemeris staged update verification was canceled."));
        }
        cleanupCanceledStagingRoot(request, activeSnapshot, result);
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
        cleanupCanceledStagingRoot(request, activeSnapshot, result);
        return result;
    }

    const EphemerisDataManifest::Profile* profile = request.manifest->profile(profileId);
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
        activationCacheRoot(pathFromQString(request.writableCacheRoot), revisionToken, activeSnapshot);

    std::vector<std::pair<std::string, std::filesystem::path>> activePaths;
    activePaths.reserve(profile->assetIds.size());
    for (const std::string& assetId : profile->assetIds) {
        const EphemerisDataManifest::Asset* asset = request.manifest->asset(assetId);
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
            skygate::ephemeris::EphemerisDataActivation::activate(activationRequest);
        result.activationStatus = activationResult.status;
        if (activationResult.status == skygate::ephemeris::EphemerisDataActivationStatus::Canceled) {
            result.status = StagedUpdateActivationStatus::Canceled;
            addDiagnostics(result, activationResult.diagnostics);
            if (result.diagnostics.empty()) {
                addDiagnostic(result, QStringLiteral("Ephemeris staged update activation was canceled."));
            }
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, activeSnapshot, result);
            }
            cleanupCanceledStagingRoot(request, activeSnapshot, result);
            return result;
        }
        if (!activationResult.isSuccess()) {
            result.status = StagedUpdateActivationStatus::ActivationFailed;
            addDiagnostics(result, activationResult.diagnostics);
            if (result.diagnostics.empty()) {
                addDiagnostic(result, QStringLiteral("Ephemeris staged update asset activation failed."));
            }
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, activeSnapshot, result);
            }
            return result;
        }

        activePaths.emplace_back(asset->id, activationResult.activePath);
        result.activatedAssetIds.push_back(stringToQString(asset->id));
        if (isCanceled()) {
            result.status = StagedUpdateActivationStatus::Canceled;
            addDiagnostic(result, QStringLiteral("Ephemeris staged update activation was canceled before metadata."));
            if (request.cleanupFailedActivationCache) {
                cleanupInactiveActivationRoot(revisionCacheRoot, activeSnapshot, result);
            }
            cleanupCanceledStagingRoot(request, activeSnapshot, result);
            return result;
        }
    }

    EphemerisDataCacheSnapshot newSnapshot =
        cacheSnapshotForActivatedProfile(*request.manifest, *profile, activePaths, activeSnapshot, revisionToken);
    newSnapshot = normalizedSnapshot(std::move(newSnapshot));
    if (settingsStore == nullptr || !settingsStore->saveEphemerisDataCache(newSnapshot)) {
        result.status = StagedUpdateActivationStatus::PersistenceFailed;
        addDiagnostic(result, QStringLiteral("Unable to persist activated ephemeris data metadata."));
        if (request.cleanupFailedActivationCache) {
            cleanupInactiveActivationRoot(revisionCacheRoot, activeSnapshot, result);
        }
        return result;
    }

    result.cacheSnapshot = std::move(newSnapshot);
    result.status = StagedUpdateActivationStatus::Activated;
    return result;
}
