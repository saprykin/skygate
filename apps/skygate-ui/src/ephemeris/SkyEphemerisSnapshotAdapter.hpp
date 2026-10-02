#pragma once

#include "SkySettingsStore.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/IEphemerisDataSnapshot.hpp"

#include <QString>

#include <optional>
#include <string_view>

class SkyEphemerisSnapshotAdapter final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    SkyEphemerisSnapshotAdapter(
        SkySettingsStore::EphemerisDataCacheSnapshot cacheSnapshot,
        bool installedDataActive,
        const skygate::ephemeris::EphemerisDataManifest* bundledFallbackManifest,
        QString bundledFallbackResourceRoot,
        QString bundledFallbackProfileId
    );

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override;
    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTDataAsset() const override;
    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationDataAsset() const override;
    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisKernelDataAsset>
    solarSystemKernelAsset(std::string_view assetId) const override;

private:
    SkySettingsStore::EphemerisDataCacheSnapshot m_cacheSnapshot;
    const skygate::ephemeris::EphemerisDataManifest* m_bundledFallbackManifest = nullptr;
    QString m_bundledFallbackResourceRoot;
    QString m_bundledFallbackProfileId;
    bool m_installedDataActive = false;
};
