#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <optional>

class SkySettingsStore final {
public:
    struct StateSnapshot final {
        bool live = true;
        bool utcDateLocked = true;
        bool utcTimeLocked = true;
        bool timelineToolbarCollapsed = false;
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
        QString projectionTypeText;
        int catalogPresetIndex = 0;
        QString catalogUrlText;
    };

    struct CatalogCacheSnapshot final {
        QString sourceLabel;
        QByteArray catalogRows;
        QByteArray constellationLineRows;
        QByteArray constellationLabelRows;
        int constellationLineSchemaVersion = 0;
        std::size_t constellationCount = 0;
    };

public:
    [[nodiscard]] bool saveState(const StateSnapshot& snapshot) const;
    [[nodiscard]] std::optional<StateSnapshot> loadState() const;
    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool saveCatalogCache(const CatalogCacheSnapshot& snapshot) const;
    [[nodiscard]] std::optional<CatalogCacheSnapshot> loadCatalogCache() const;
};
