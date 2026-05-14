#pragma once

#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/EphemerisDataActivation.hpp"
#include "skygate/ephemeris/EphemerisDataSnapshot.hpp"

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
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
        PersistenceFailed
    };

    struct StagedUpdateActivationRequest final {
        const skygate::ephemeris::EphemerisDataManifest* manifest = nullptr;
        QString profileId;
        QString stagedResourceRoot;
        QString writableCacheRoot;
        QString revisionToken;
        std::vector<skygate::ephemeris::EphemerisDataManifestAssetKind> requiredKinds;
        std::vector<skygate::ephemeris::EphemerisStagedUpdateVerificationRequest::ExpectedComponent> expectedComponents;
        bool allowQtResourceKernelAssets = false;
        std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
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
    [[nodiscard]] QString dataRevisionToken() const;
    [[nodiscard]] bool usingInstalledData() const noexcept;
    [[nodiscard]] std::uint64_t dataRevision() const noexcept;
    [[nodiscard]] SkySettingsStore::EphemerisDataCacheSnapshot activeCacheSnapshot() const;
    [[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot() const noexcept;

    [[nodiscard]] bool restoreFromSettings();
    [[nodiscard]] bool clearInstalledDataCache();
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
    QString m_statusText;
    QString m_datasetInfoText;
    ActiveSource m_activeSource = ActiveSource::BundledFallback;
    std::uint64_t m_dataRevision = 1;
};
