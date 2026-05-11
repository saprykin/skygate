#pragma once

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include "SkyOverlayLayerVisibility.hpp"

#include <cstddef>
#include <optional>

class SkySettingsStore final {
public:
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
        qint64 utcEpochSeconds = 0;
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
    };

    struct CatalogCacheSnapshot final {
        QString sourceLabel;
        QByteArray catalogPayload;
        QString deepSkySourceLabel;
        QByteArray deepSkyCatalogPayload;
        QByteArray constellationLineRows;
        QByteArray constellationLabelRows;
        int constellationLineSchemaVersion = 0;
        std::size_t constellationCount = 0;
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
};
