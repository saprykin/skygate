#pragma once

#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/EphemerisDataActivation.hpp"
#include "skygate/ephemeris/EphemerisDataSnapshot.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class SkySettingsStore;

class SkyEphemerisDataManager final : public QObject {
    Q_OBJECT

public:
    enum class StagedUpdateActivationStatus : std::uint8_t {
        Activated,
        InvalidRequest,
        VerificationFailed,
        ActivationFailed,
        PersistenceFailed,
        Canceled
    };

    enum class StagedUpdateDownloadStatus : std::uint8_t {
        Downloaded,
        InvalidRequest,
        MissingSource,
        IoError,
        Canceled
    };

    struct StagedUpdateDownloadRequest final {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = nullptr;
        QString sourceUrl;
        QString sourceResourceRoot;
        QString stagedResourceRoot;
        std::function<bool()> cancellationRequested;
        std::function<void(std::uint64_t stagedBytes, std::optional<std::uint64_t> totalBytes)> progressHandler;
        bool retainPartialStagingOnCancellation = true;
    };

    struct StagedUpdateDownloadResult final {
        StagedUpdateDownloadStatus status = StagedUpdateDownloadStatus::InvalidRequest;
        QString stagedPath;
        std::uint64_t stagedBytes = 0U;
        std::vector<QString> diagnostics;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == StagedUpdateDownloadStatus::Downloaded;
        }
    };

    struct StagedUpdateActivationRequest final {
        const skygate::ephemeris::EphemerisDataManifest* manifest = nullptr;
        QString profileId;
        QString stagedResourceRoot;
        QString writableCacheRoot;
        QString revisionToken;
        std::vector<skygate::ephemeris::EphemerisDataManifestAssetKind> requiredKinds;
        std::vector<skygate::ephemeris::EphemerisStagedUpdateVerificationRequest::ExpectedComponent> expectedComponents;
        std::function<bool()> cancellationRequested;
        bool allowQtResourceKernelAssets = false;
        std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
        bool retainStagedResourcesOnCancellation = true;
        bool cleanupFailedActivationCache = true;
    };

    struct StagedUpdateActivationResult final {
        StagedUpdateActivationStatus status = StagedUpdateActivationStatus::InvalidRequest;
        skygate::ephemeris::EphemerisStagedUpdateVerificationStatus verificationStatus =
            skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::InvalidRequest;
        skygate::ephemeris::EphemerisDataActivationStatus activationStatus =
            skygate::ephemeris::EphemerisDataActivationStatus::InvalidRequest;
        SkySettingsStore::EphemerisDataCacheSnapshot cacheSnapshot;
        std::vector<QString> diagnostics;
        std::vector<QString> activatedAssetIds;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == StagedUpdateActivationStatus::Activated;
        }
    };

    explicit SkyEphemerisDataManager(SkySettingsStore* settingsStore, QObject* parent = nullptr);
    ~SkyEphemerisDataManager() override;

    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString datasetInfoText() const;
    [[nodiscard]] QString modernKernelStatusText() const;
    [[nodiscard]] QString longRangeKernelStatusText() const;
    [[nodiscard]] QString earthOrientationStatusText() const;
    [[nodiscard]] QString leapSecondStatusText() const;
    [[nodiscard]] QString deltaTStatusText() const;
    [[nodiscard]] QString lastUpdateResultText() const;
    [[nodiscard]] QString dataRevisionToken() const;
    [[nodiscard]] bool usingInstalledData() const noexcept;
    [[nodiscard]] std::uint64_t dataRevision() const noexcept;
    [[nodiscard]] SkySettingsStore::EphemerisDataCacheSnapshot activeCacheSnapshot() const;
    [[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot() const noexcept;

    void setBundledFallbackData(
        const skygate::ephemeris::EphemerisDataManifest* manifest, QString resourceRoot, QString profileId = {}
    );
    [[nodiscard]] bool restoreFromSettings();
    [[nodiscard]] bool clearInstalledDataCache();
    void requestUpdateCancellation() noexcept;
    void clearUpdateCancellation() noexcept;
    [[nodiscard]] bool updateCancellationRequested() const noexcept;
    [[nodiscard]] StagedUpdateDownloadResult stageEphemerisUpdateAsset(const StagedUpdateDownloadRequest& request);
    [[nodiscard]] StagedUpdateActivationResult
    activateVerifiedStagedUpdateSet(const StagedUpdateActivationRequest& request);

signals:
    void statusTextChanged();
    void activeDataChanged();
    void dataRevisionChanged();

private:
    enum class ActiveSource : std::uint8_t {
        BundledFallback,
        Installed,
        MissingInstalledFallback
    };

    void applyCacheSnapshot(
        SkySettingsStore::EphemerisDataCacheSnapshot cacheSnapshot,
        ActiveSource source,
        QString statusText,
        bool emitSignals
    );

private:
    SkySettingsStore* m_settingsStore = nullptr;
    SkySettingsStore::EphemerisDataCacheSnapshot m_activeCacheSnapshot;
    std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> m_activeDataSnapshot;
    const skygate::ephemeris::EphemerisDataManifest* m_bundledFallbackManifest = nullptr;
    QString m_bundledFallbackResourceRoot;
    QString m_bundledFallbackProfileId;
    QString m_statusText;
    QString m_datasetInfoText;
    ActiveSource m_activeSource = ActiveSource::BundledFallback;
    std::uint64_t m_dataRevision = 1;
    std::atomic_bool m_updateCancellationRequested = false;
};
