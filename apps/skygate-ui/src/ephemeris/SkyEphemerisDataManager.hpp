#pragma once

#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/EphemerisDataSnapshot.hpp"

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>

class SkySettingsStore;

class SkyEphemerisDataManager final : public QObject {
    Q_OBJECT

public:
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
