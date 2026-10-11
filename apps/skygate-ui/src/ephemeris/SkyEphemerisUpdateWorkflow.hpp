#pragma once

#include "engine/EphemerisDataManifest.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class SkyEphemerisDataManager;

// Runs one ephemeris data update: manifest refresh, profile asset downloads
// through `SkyEphemerisDataManager`, progress accounting, and verified
// activation. Download continuations hold a weak pointer to the workflow, so
// destroying it while a transfer is in flight drops later completions
// without touching it. The facade forwards its QML properties here and keeps
// the persisted user settings.
class SkyEphemerisUpdateWorkflow final : public QObject {
    Q_OBJECT

public:
    SkyEphemerisUpdateWorkflow(
        SkyEphemerisDataManager* dataManager,
        const skygate::ephemeris::EphemerisDataManifest* bundledFallbackManifest,
        QString updateResourceRoot,
        QString writableCacheRoot,
        QObject* parent = nullptr
    );
    ~SkyEphemerisUpdateWorkflow() override;

    [[nodiscard]] const skygate::ephemeris::EphemerisDataManifest* activeManifest() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString earthOrientationStatusText() const;
    [[nodiscard]] QString leapSecondStatusText() const;
    [[nodiscard]] QString deltaTStatusText() const;
    [[nodiscard]] bool updateEnabled() const noexcept;
    [[nodiscard]] bool isUpdateInProgress() const noexcept;
    [[nodiscard]] double updateProgress() const noexcept;

    [[nodiscard]] bool updateProfile(const QString& profileId, const QString& manifestUrl);
    [[nodiscard]] bool checkSupportDataUpdates(const QString& manifestUrl);
    void cancelActiveUpdate();
    [[nodiscard]] bool clearInstalledDataCache();
    [[nodiscard]] bool clearPlanetaryKernelCache();
    [[nodiscard]] bool clearSupportDataCache();

signals:
    void statusChanged();
    void preferredProfileSelected(const QString& profileId);

private:
    [[nodiscard]] bool updateCancellationRequested() const noexcept;
    void setStatusText(QString statusText);
    void setUpdateProgress(double progress) noexcept;
    void refreshManifestAsync(
        const QString& manifestUrl, const QString& stagedRoot, std::function<void(bool)> completionHandler
    );
    void finishUpdate(bool success, QString statusText);
    void continueUpdate(const QString& profileId, const QString& stagedRoot);

private:
    SkyEphemerisDataManager* m_dataManager = nullptr;
    const skygate::ephemeris::EphemerisDataManifest* m_bundledFallbackManifest = nullptr;
    std::optional<skygate::ephemeris::EphemerisDataManifest> m_refreshedManifest;
    QString m_updateResourceRoot;
    QString m_writableCacheRoot;
    QString m_statusText;
    QString m_availableEarthOrientationVersion;
    QString m_availableLeapSecondVersion;
    QString m_availableDeltaTVersion;
    bool m_supportDataUpdateChecked = false;
    bool m_updateInProgress = false;
    double m_updateProgress = 0.0;
};
