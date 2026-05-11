#pragma once

#include "SkyOverlayLayerVisibility.hpp"
#include "SkySettingsStore.hpp"

#include <QString>
#include <QtGlobal>

#include <optional>

class QSettings;

namespace skygate::ui::internal {

struct SkyTimelineSettingsSnapshot final {
    bool live = true;
    bool toolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = 60;
    qint64 utcEpochSeconds = 0;
};

struct SkySearchSettingsSnapshot final {
    bool toolbarCollapsed = false;
};

struct SkyViewSettingsSnapshot final {
    double magnitudeCutoff = 6.0;
    double centerAltitudeDeg = 0.0;
    double centerAzimuthDeg = 0.0;
    double fieldOfViewDeg = 100.0;
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
};

[[nodiscard]] SkyStateSettingsSnapshot splitStateSnapshot(
    const SkySettingsStore::StateSnapshot& snapshot
);
[[nodiscard]] SkySettingsStore::StateSnapshot mergeStateSnapshot(
    const SkyStateSettingsSnapshot& snapshot
);
void saveStateSnapshot(QSettings& settings, const SkySettingsStore::StateSnapshot& snapshot);
[[nodiscard]] std::optional<SkySettingsStore::StateSnapshot> loadStateSnapshot(
    QSettings& settings
);

}  // namespace skygate::ui::internal
