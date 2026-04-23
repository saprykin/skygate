#include "SkySettingsStore.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

using namespace skygate::ui::internal;

bool SkySettingsStore::saveState(const StateSnapshot& snapshot) const
{
    QSettings settings;
    settings.setValue(
        SkyContextSettings::key("version"),
        SkyContextControllerConstants::kSettingsVersion
    );
    settings.setValue(SkyContextSettings::key("live"), snapshot.live);
    settings.setValue(
        SkyContextSettings::key("timelineToolbarCollapsed"),
        snapshot.timelineToolbarCollapsed
    );
    settings.setValue(
        SkyContextSettings::key("searchToolbarCollapsed"),
        snapshot.searchToolbarCollapsed
    );
    settings.setValue(SkyContextSettings::key("speedMultiplier"), snapshot.speedMultiplier);
    settings.setValue(SkyContextSettings::key("stepSeconds"), snapshot.stepSeconds);
    settings.setValue(SkyContextSettings::key("magnitudeCutoff"), snapshot.magnitudeCutoff);
    settings.setValue(
        SkyContextSettings::key("viewCenterAltitudeDeg"),
        snapshot.viewCenterAltitudeDeg
    );
    settings.setValue(
        SkyContextSettings::key("viewCenterAzimuthDeg"),
        snapshot.viewCenterAzimuthDeg
    );
    settings.setValue(
        SkyContextSettings::key("viewFieldOfViewDeg"),
        snapshot.viewFieldOfViewDeg
    );
    settings.setValue(SkyContextSettings::key("utcEpochSeconds"), snapshot.utcEpochSeconds);
    settings.setValue(SkyContextSettings::key("latitudeDeg"), snapshot.latitudeDeg);
    settings.setValue(SkyContextSettings::key("longitudeDeg"), snapshot.longitudeDeg);
    settings.setValue(SkyContextSettings::key("elevationMeters"), snapshot.elevationMeters);
    settings.setValue(
        SkyContextSettings::key("locationSource"),
        snapshot.locationSourceText
    );
    settings.setValue(SkyContextSettings::key("selectedCityId"), snapshot.selectedCityId);
    settings.setValue(
        SkyContextSettings::key("projectionType"),
        snapshot.projectionTypeText
    );
    settings.setValue(SkyContextSettings::key("themeId"), snapshot.themeId);
    settings.setValue(
        SkyContextSettings::key("overlayLayers/horizon"),
        snapshot.overlayLayers.horizon
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/altAzGrid"),
        snapshot.overlayLayers.altAzGrid
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/constellationLines"),
        snapshot.overlayLayers.constellationLines
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/constellationLabels"),
        snapshot.overlayLayers.constellationLabels
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/ecliptic"),
        snapshot.overlayLayers.ecliptic
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/celestialEquator"),
        snapshot.overlayLayers.celestialEquator
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/circumpolarBoundary"),
        snapshot.overlayLayers.circumpolarBoundary
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/solarSystemLabels"),
        snapshot.overlayLayers.solarSystemLabels
    );
    settings.setValue(
        SkyContextSettings::key("catalogPresetIndex"),
        snapshot.catalogPresetIndex
    );
    settings.setValue(SkyContextSettings::key("catalogUrlText"), snapshot.catalogUrlText);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

std::optional<SkySettingsStore::StateSnapshot> SkySettingsStore::loadState() const
{
    QSettings settings;
    if (!settings.contains(SkyContextSettings::key("version"))) {
        return std::nullopt;
    }

    StateSnapshot snapshot;
    snapshot.live = settings.value(SkyContextSettings::key("live"), snapshot.live).toBool();
    snapshot.timelineToolbarCollapsed = settings.value(
        SkyContextSettings::key("timelineToolbarCollapsed"),
        snapshot.timelineToolbarCollapsed
    ).toBool();
    snapshot.searchToolbarCollapsed = settings.value(
        SkyContextSettings::key("searchToolbarCollapsed"),
        snapshot.searchToolbarCollapsed
    ).toBool();
    snapshot.speedMultiplier = settings.value(
        SkyContextSettings::key("speedMultiplier"),
        snapshot.speedMultiplier
    ).toDouble();
    snapshot.stepSeconds = settings.value(
        SkyContextSettings::key("stepSeconds"),
        snapshot.stepSeconds
    ).toInt();
    snapshot.magnitudeCutoff = settings.value(
        SkyContextSettings::key("magnitudeCutoff"),
        snapshot.magnitudeCutoff
    ).toDouble();
    snapshot.viewCenterAltitudeDeg = settings.value(
        SkyContextSettings::key("viewCenterAltitudeDeg"),
        snapshot.viewCenterAltitudeDeg
    ).toDouble();
    snapshot.viewCenterAzimuthDeg = settings.value(
        SkyContextSettings::key("viewCenterAzimuthDeg"),
        snapshot.viewCenterAzimuthDeg
    ).toDouble();
    snapshot.viewFieldOfViewDeg = settings.value(
        SkyContextSettings::key("viewFieldOfViewDeg"),
        snapshot.viewFieldOfViewDeg
    ).toDouble();
    snapshot.utcEpochSeconds = settings.value(
        SkyContextSettings::key("utcEpochSeconds"),
        snapshot.utcEpochSeconds
    ).toLongLong();
    snapshot.latitudeDeg = settings.value(
        SkyContextSettings::key("latitudeDeg"),
        snapshot.latitudeDeg
    ).toDouble();
    snapshot.longitudeDeg = settings.value(
        SkyContextSettings::key("longitudeDeg"),
        snapshot.longitudeDeg
    ).toDouble();
    snapshot.elevationMeters = settings.value(
        SkyContextSettings::key("elevationMeters"),
        snapshot.elevationMeters
    ).toDouble();
    snapshot.locationSourceText = settings.value(
        SkyContextSettings::key("locationSource"),
        snapshot.locationSourceText
    ).toString();
    snapshot.selectedCityId = settings.value(
        SkyContextSettings::key("selectedCityId"),
        snapshot.selectedCityId
    ).toString();
    snapshot.projectionTypeText = settings.value(
        SkyContextSettings::key("projectionType"),
        snapshot.projectionTypeText
    ).toString();
    snapshot.themeId = settings.value(
        SkyContextSettings::key("themeId"),
        snapshot.themeId
    ).toString();
    snapshot.overlayLayers.horizon = settings.value(
        SkyContextSettings::key("overlayLayers/horizon"),
        snapshot.overlayLayers.horizon
    ).toBool();
    snapshot.overlayLayers.altAzGrid = settings.value(
        SkyContextSettings::key("overlayLayers/altAzGrid"),
        snapshot.overlayLayers.altAzGrid
    ).toBool();
    snapshot.overlayLayers.constellationLines = settings.value(
        SkyContextSettings::key("overlayLayers/constellationLines"),
        snapshot.overlayLayers.constellationLines
    ).toBool();
    snapshot.overlayLayers.constellationLabels = settings.value(
        SkyContextSettings::key("overlayLayers/constellationLabels"),
        snapshot.overlayLayers.constellationLabels
    ).toBool();
    snapshot.overlayLayers.ecliptic = settings.value(
        SkyContextSettings::key("overlayLayers/ecliptic"),
        snapshot.overlayLayers.ecliptic
    ).toBool();
    snapshot.overlayLayers.celestialEquator = settings.value(
        SkyContextSettings::key("overlayLayers/celestialEquator"),
        snapshot.overlayLayers.celestialEquator
    ).toBool();
    snapshot.overlayLayers.circumpolarBoundary = settings.value(
        SkyContextSettings::key("overlayLayers/circumpolarBoundary"),
        snapshot.overlayLayers.circumpolarBoundary
    ).toBool();
    snapshot.overlayLayers.solarSystemLabels = settings.value(
        SkyContextSettings::key("overlayLayers/solarSystemLabels"),
        snapshot.overlayLayers.solarSystemLabels
    ).toBool();
    snapshot.catalogPresetIndex = settings.value(
        SkyContextSettings::key("catalogPresetIndex"),
        snapshot.catalogPresetIndex
    ).toInt();
    snapshot.catalogUrlText = settings.value(
        SkyContextSettings::key("catalogUrlText"),
        snapshot.catalogUrlText
    ).toString();
    return snapshot;
}

bool SkySettingsStore::clearCatalogCache() const
{
    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
    ).toString();
    const QString defaultPath = SkyContextSettings::defaultCatalogCachePath();

    QStringList cachePaths;
    const auto appendCachePath = [&cachePaths](const QString& path) {
        if (!path.isEmpty() && !cachePaths.contains(path)) {
            cachePaths.push_back(path);
        }
    };

    appendCachePath(configuredPath);
    appendCachePath(defaultPath);

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries = defaultDir.entryList(
            QStringList {"catalog-cache*.txt"},
            QDir::Files | QDir::Readable
        );
        for (const QString& entryName : matchingEntries) {
            appendCachePath(defaultDir.filePath(entryName));
        }
    }

    bool removedAllCacheFiles = true;
    for (const QString& cachePath : cachePaths) {
        QFile cacheFile(cachePath);
        if (cacheFile.exists() && !cacheFile.remove()) {
            removedAllCacheFiles = false;
        }
    }

    settings.remove(SkyContextSettings::key("catalogCachePath"));
    settings.remove(SkyContextSettings::key("catalogSourceLabel"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLabelRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogConstellationCount"));
    settings.sync();
    return removedAllCacheFiles && settings.status() == QSettings::NoError;
}

bool SkySettingsStore::saveCatalogCache(const CatalogCacheSnapshot& snapshot) const
{
    if (snapshot.catalogPayload.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return false;
    }

    const QFileInfo targetInfo(configuredPath);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        return false;
    }

    QSaveFile cacheFile(configuredPath);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const qint64 writtenBytes = cacheFile.write(snapshot.catalogPayload);
    if (writtenBytes != snapshot.catalogPayload.size()) {
        cacheFile.cancelWriting();
        return false;
    }

    if (!cacheFile.commit()) {
        return false;
    }

    settings.setValue(
        SkyContextSettings::key("version"),
        SkyContextControllerConstants::kSettingsVersion
    );
    settings.setValue(SkyContextSettings::key("catalogCachePath"), configuredPath);
    settings.setValue(
        SkyContextSettings::key("catalogSourceLabel"),
        snapshot.sourceLabel
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLineRefs"),
        snapshot.constellationLineRows
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLabelRefs"),
        snapshot.constellationLabelRows
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLineSchemaVersion"),
        snapshot.constellationLineSchemaVersion
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationCount"),
        static_cast<qulonglong>(snapshot.constellationCount)
    );
    settings.sync();
    return settings.status() == QSettings::NoError;
}

std::optional<SkySettingsStore::CatalogCacheSnapshot> SkySettingsStore::loadCatalogCache() const
{
    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return std::nullopt;
    }

    QFile cacheFile(configuredPath);
    if (!cacheFile.exists() || !cacheFile.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    CatalogCacheSnapshot snapshot;
    snapshot.catalogPayload = cacheFile.readAll();
    if (snapshot.catalogPayload.isEmpty()) {
        return std::nullopt;
    }

    snapshot.sourceLabel = settings.value(
        SkyContextSettings::key("catalogSourceLabel"),
        QString("Saved")
    ).toString();
    snapshot.constellationLineRows = settings.value(
        SkyContextSettings::key("catalogConstellationLineRefs")
    ).toByteArray();
    snapshot.constellationLabelRows = settings.value(
        SkyContextSettings::key("catalogConstellationLabelRefs")
    ).toByteArray();
    snapshot.constellationLineSchemaVersion = settings.value(
        SkyContextSettings::key("catalogConstellationLineSchemaVersion"),
        0
    ).toInt();
    snapshot.constellationCount = static_cast<std::size_t>(
        settings.value(
            SkyContextSettings::key("catalogConstellationCount"),
            static_cast<qulonglong>(0)
        ).toULongLong()
    );
    return snapshot;
}
