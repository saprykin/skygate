#pragma once

#include "SkyEphemerisUserSettingsSnapshot.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyPersistedStateSnapshot.hpp"

#include <QString>
#include <QtGlobal>

#include <optional>

class QSettings;

namespace skygate::ui::internal {

struct SkyTimelineSettingsSnapshot final {
    bool live = true;
    bool toolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = SkyPersistedStateSnapshot::kDefaultStepSeconds;
    qint64 utcEpochMicros = 0;
};

struct SkySearchSettingsSnapshot final {
    bool toolbarCollapsed = false;
};

struct SkyViewSettingsSnapshot final {
    double magnitudeCutoff = SkyPersistedStateSnapshot::kDefaultMagnitudeCutoff;
    double centerAltitudeDeg = SkyPersistedStateSnapshot::kLegacyViewCenterAltitudeDeg;
    double centerAzimuthDeg = SkyPersistedStateSnapshot::kLegacyViewCenterAzimuthDeg;
    double fieldOfViewDeg = SkyPersistedStateSnapshot::kDefaultFieldOfViewDeg;
    QString projectionTypeText;
    QString themeId;
};

struct SkyLocationSettingsSnapshot final {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;
    QString sourceText;
    QString selectedCityId;
    QString displayTimeZoneId;
};

struct SkyCatalogSourceSettingsSnapshot final {
    int starPresetIndex = 0;
    QString starUrlText;
    int deepSkyPresetIndex = 0;
    QString deepSkyUrlText;
};

struct SkyLoggingSettingsSnapshot final {
    bool logToTerminal = true;
    bool logToFile = false;
    QString logFilePath;
};

struct SkyStateSettingsSnapshot final {
    SkyTimelineSettingsSnapshot timeline;
    SkySearchSettingsSnapshot search;
    SkyViewSettingsSnapshot view;
    SkyLocationSettingsSnapshot location;
    SkyOverlayLayerVisibility overlayLayers;
    SkyCatalogSourceSettingsSnapshot catalogSources;
    SkyLoggingSettingsSnapshot logging;
    SkyEphemerisUserSettingsSnapshot ephemeris;
    bool ephemerisSettingsPresent = false;
};

[[nodiscard]] SkyStateSettingsSnapshot splitStateSnapshot(const SkyPersistedStateSnapshot& snapshot);
[[nodiscard]] SkyPersistedStateSnapshot mergeStateSnapshot(const SkyStateSettingsSnapshot& snapshot);
void saveStateSnapshot(QSettings& settings, const SkyPersistedStateSnapshot& snapshot);
[[nodiscard]] std::optional<SkyPersistedStateSnapshot> loadStateSnapshot(QSettings& settings);

}  // namespace skygate::ui::internal
