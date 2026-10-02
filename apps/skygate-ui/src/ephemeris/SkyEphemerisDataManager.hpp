#pragma once

#include "SkyEphemerisCacheController.hpp"
#include "SkyEphemerisDownloadService.hpp"
#include "SkySettingsStore.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

class SkySettingsStore;

class SkyEphemerisDataManager final : public QObject {
    Q_OBJECT

public:
    using StagedUpdateActivationStatus = SkyEphemerisCacheController::StagedUpdateActivationStatus;
    using StagedUpdateDownloadStatus = SkyEphemerisDownloadService::StagedUpdateDownloadStatus;
    using StagedUpdateDownloadRequest = SkyEphemerisDownloadService::StagedUpdateDownloadRequest;
    using StagedUpdateDownloadResult = SkyEphemerisDownloadService::StagedUpdateDownloadResult;
    using StagedUpdateActivationRequest = SkyEphemerisCacheController::StagedUpdateActivationRequest;
    using StagedUpdateActivationResult = SkyEphemerisCacheController::StagedUpdateActivationResult;

    explicit SkyEphemerisDataManager(SkySettingsStore* settingsStore, QObject* parent = nullptr);
    ~SkyEphemerisDataManager() override;

    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString datasetInfoText() const;
    [[nodiscard]] QString shortRangeKernelStatusText() const;
    [[nodiscard]] QString longRangeKernelStatusText() const;
    [[nodiscard]] QString earthOrientationStatusText() const;
    [[nodiscard]] QString leapSecondStatusText() const;
    [[nodiscard]] QString deltaTStatusText() const;
    [[nodiscard]] std::uint64_t planetaryKernelCacheSizeBytes() const;
    [[nodiscard]] std::uint64_t supportDataCacheSizeBytes() const;
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
    [[nodiscard]] bool clearPlanetaryKernelCache();
    [[nodiscard]] bool clearSupportDataCache();
    void requestUpdateCancellation() noexcept;
    void clearUpdateCancellation() noexcept;
    [[nodiscard]] bool updateCancellationRequested() const noexcept;

    [[nodiscard]] StagedUpdateDownloadResult stageEphemerisUpdateAsset(const StagedUpdateDownloadRequest& request);
    void stageEphemerisUpdateAssetAsync(
        const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
    );
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
    std::unique_ptr<SkyEphemerisDownloadService> m_downloadService;
    SkyEphemerisCacheController m_cacheController;
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
