#pragma once

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include "SkyOverlayLayerVisibility.hpp"
#include "Types.hpp"

#include <cstddef>
#include <optional>

class SkySettingsStore final {
public:
    struct EphemerisUserSettingsSnapshot final {
        skygate::ephemeris::EphemerisEngineKind engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
        skygate::ephemeris::EphemerisCorrectionFlags correctionFlags =
            skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
        QString correctionPresetId = QStringLiteral("apparent-topocentric");
        bool fallbackToSimpleEngine = true;
        bool refractionEnabled = true;
        double atmosphericPressureHpa = 1013.25;
        double atmosphericTemperatureC = 10.0;
        double relativeHumidity = 0.0;
        double observingWavelengthMicrometers = 0.55;
        QString preferredDataProfileId = QStringLiteral("de440s-short-range");
        bool onlineUpdatesEnabled = true;
        QString updatePresetId = QStringLiteral("bundled");
        QString updateManifestUrl;
    };

    struct StateSnapshot final {
        bool live = true;
        bool timelineToolbarCollapsed = false;
        bool searchToolbarCollapsed = false;
        double speedMultiplier = 1.0;
        int stepSeconds = 60;
        double magnitudeCutoff = 6.0;
        double viewCenterAltitudeDeg = 0.0;
        double viewCenterAzimuthDeg = 0.0;
        double viewFieldOfViewDeg = 100.0;
        qint64 utcEpochMicros = 0;
        double latitudeDeg = 0.0;
        double longitudeDeg = 0.0;
        double elevationMeters = 0.0;
        QString locationSourceText;
        QString selectedCityId;
        QString displayTimeZoneId;
        QString projectionTypeText;
        QString themeId;
        SkyOverlayLayerVisibility overlayLayers;
        int catalogPresetIndex = 0;
        QString catalogUrlText;
        int deepSkyCatalogPresetIndex = 0;
        QString deepSkyCatalogUrlText;
        bool logToTerminal = true;
        bool logToFile = false;
        QString logFilePath;
        EphemerisUserSettingsSnapshot ephemeris;
        bool ephemerisSettingsPresent = false;
    };

    struct CatalogCacheSnapshot final {
        QString sourceLabel;
        QByteArray catalogPayload;
        QString deepSkySourceLabel;
        QByteArray deepSkyCatalogPayload;
        QByteArray constellationLineRows;
        QByteArray constellationAnchorGroupRows;
        int constellationLineSchemaVersion = 0;
        std::size_t constellationCount = 0;
    };

    struct EphemerisDataCacheSnapshot final {
        QString installedKernelAssetId;
        QString installedKernelProfileId;
        QString installedKernelPath;
        QString installedKernelVersion;
        QString installedEarthOrientationPath;
        QString installedEarthOrientationVersion;
        QString installedLeapSecondTablePath;
        QString installedLeapSecondTableVersion;
        QString installedDeltaTDataPath;
        QString installedDeltaTDataVersion;
        QString dataRevisionToken = QStringLiteral("bundled");
        QString lastUpdateResult = QStringLiteral("Bundled fallback");
    };

public:
    [[nodiscard]] bool saveState(const StateSnapshot& snapshot) const;
    [[nodiscard]] std::optional<StateSnapshot> loadState() const;
    [[nodiscard]] bool saveMainWindowSize(const QSize& size) const;
    [[nodiscard]] QSize loadMainWindowSize() const;
    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
    [[nodiscard]] bool saveCatalogCache(const CatalogCacheSnapshot& snapshot) const;
    [[nodiscard]] std::optional<CatalogCacheSnapshot> loadCatalogCache() const;
    [[nodiscard]] bool saveEphemerisDataCache(const EphemerisDataCacheSnapshot& snapshot) const;
    [[nodiscard]] EphemerisDataCacheSnapshot loadEphemerisDataCache() const;
    [[nodiscard]] bool clearEphemerisDataCache() const;
};
