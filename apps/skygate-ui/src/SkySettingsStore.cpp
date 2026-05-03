#include "SkySettingsStore.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

#include <cmath>
#include <optional>

using namespace skygate::ui::internal;

namespace {

void appendCachePath(QStringList& cachePaths, const QString& path)
{
    if (!path.isEmpty() && !cachePaths.contains(path)) {
        cachePaths.push_back(path);
    }
}

bool removeCacheFiles(const QStringList& cachePaths)
{
    bool removedAllCacheFiles = true;
    for (const QString& cachePath : cachePaths) {
        QFile cacheFile(cachePath);
        if (cacheFile.exists() && !cacheFile.remove()) {
            removedAllCacheFiles = false;
        }
    }
    return removedAllCacheFiles;
}

std::optional<bool> boolFromVariant(const QVariant& value)
{
    if (!value.isValid()) {
        return std::nullopt;
    }
    if (value.metaType().id() == QMetaType::Bool) {
        return value.toBool();
    }

    const QString text = value.toString().trimmed().toLower();
    if (text == "true" || text == "1" || text == "yes" || text == "on") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no" || text == "off") {
        return false;
    }
    return std::nullopt;
}

bool readBoolSetting(QSettings& settings, const QString& key, const bool fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }
    return boolFromVariant(settings.value(key)).value_or(fallback);
}

double readDoubleSetting(QSettings& settings, const QString& key, const double fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const double value = settings.value(key).toDouble(&ok);
    return ok && std::isfinite(value) ? value : fallback;
}

int readIntSetting(QSettings& settings, const QString& key, const int fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const int value = settings.value(key).toInt(&ok);
    return ok ? value : fallback;
}

qint64 readLongLongSetting(QSettings& settings, const QString& key, const qint64 fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const qint64 value = settings.value(key).toLongLong(&ok);
    return ok ? value : fallback;
}

qulonglong readULongLongSetting(
    QSettings& settings,
    const QString& key,
    const qulonglong fallback
)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const qulonglong value = settings.value(key).toULongLong(&ok);
    return ok ? value : fallback;
}

} // namespace

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
        SkyContextSettings::key("overlayLayers/deepSkyObjects"),
        snapshot.overlayLayers.deepSkyObjects
    );
    settings.setValue(
        SkyContextSettings::key("overlayLayers/deepSkyLabels"),
        snapshot.overlayLayers.deepSkyLabels
    );
    settings.setValue(
        SkyContextSettings::key("catalogPresetIndex"),
        snapshot.catalogPresetIndex
    );
    settings.setValue(SkyContextSettings::key("catalogUrlText"), snapshot.catalogUrlText);
    settings.setValue(
        SkyContextSettings::key("deepSkyCatalogPresetIndex"),
        snapshot.deepSkyCatalogPresetIndex
    );
    settings.setValue(
        SkyContextSettings::key("deepSkyCatalogUrlText"),
        snapshot.deepSkyCatalogUrlText
    );
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
    snapshot.live = readBoolSetting(settings, SkyContextSettings::key("live"), snapshot.live);
    snapshot.timelineToolbarCollapsed = readBoolSetting(
        settings,
        SkyContextSettings::key("timelineToolbarCollapsed"),
        snapshot.timelineToolbarCollapsed
    );
    snapshot.searchToolbarCollapsed = readBoolSetting(
        settings,
        SkyContextSettings::key("searchToolbarCollapsed"),
        snapshot.searchToolbarCollapsed
    );
    snapshot.speedMultiplier = readDoubleSetting(
        settings,
        SkyContextSettings::key("speedMultiplier"),
        snapshot.speedMultiplier
    );
    snapshot.stepSeconds = readIntSetting(
        settings,
        SkyContextSettings::key("stepSeconds"),
        snapshot.stepSeconds
    );
    snapshot.magnitudeCutoff = readDoubleSetting(
        settings,
        SkyContextSettings::key("magnitudeCutoff"),
        snapshot.magnitudeCutoff
    );
    snapshot.viewCenterAltitudeDeg = readDoubleSetting(
        settings,
        SkyContextSettings::key("viewCenterAltitudeDeg"),
        snapshot.viewCenterAltitudeDeg
    );
    snapshot.viewCenterAzimuthDeg = readDoubleSetting(
        settings,
        SkyContextSettings::key("viewCenterAzimuthDeg"),
        snapshot.viewCenterAzimuthDeg
    );
    snapshot.viewFieldOfViewDeg = readDoubleSetting(
        settings,
        SkyContextSettings::key("viewFieldOfViewDeg"),
        snapshot.viewFieldOfViewDeg
    );
    snapshot.utcEpochSeconds = readLongLongSetting(
        settings,
        SkyContextSettings::key("utcEpochSeconds"),
        snapshot.utcEpochSeconds
    );
    snapshot.latitudeDeg = readDoubleSetting(
        settings,
        SkyContextSettings::key("latitudeDeg"),
        snapshot.latitudeDeg
    );
    snapshot.longitudeDeg = readDoubleSetting(
        settings,
        SkyContextSettings::key("longitudeDeg"),
        snapshot.longitudeDeg
    );
    snapshot.elevationMeters = readDoubleSetting(
        settings,
        SkyContextSettings::key("elevationMeters"),
        snapshot.elevationMeters
    );
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
    snapshot.overlayLayers.horizon = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/horizon"),
        snapshot.overlayLayers.horizon
    );
    snapshot.overlayLayers.altAzGrid = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/altAzGrid"),
        snapshot.overlayLayers.altAzGrid
    );
    snapshot.overlayLayers.constellationLines = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/constellationLines"),
        snapshot.overlayLayers.constellationLines
    );
    snapshot.overlayLayers.constellationLabels = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/constellationLabels"),
        snapshot.overlayLayers.constellationLabels
    );
    snapshot.overlayLayers.ecliptic = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/ecliptic"),
        snapshot.overlayLayers.ecliptic
    );
    snapshot.overlayLayers.celestialEquator = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/celestialEquator"),
        snapshot.overlayLayers.celestialEquator
    );
    snapshot.overlayLayers.circumpolarBoundary = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/circumpolarBoundary"),
        snapshot.overlayLayers.circumpolarBoundary
    );
    snapshot.overlayLayers.solarSystemLabels = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/solarSystemLabels"),
        snapshot.overlayLayers.solarSystemLabels
    );
    snapshot.overlayLayers.deepSkyObjects = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/deepSkyObjects"),
        snapshot.overlayLayers.deepSkyObjects
    );
    snapshot.overlayLayers.deepSkyLabels = readBoolSetting(
        settings,
        SkyContextSettings::key("overlayLayers/deepSkyLabels"),
        snapshot.overlayLayers.deepSkyLabels
    );
    snapshot.catalogPresetIndex = readIntSetting(
        settings,
        SkyContextSettings::key("catalogPresetIndex"),
        snapshot.catalogPresetIndex
    );
    snapshot.catalogUrlText = settings.value(
        SkyContextSettings::key("catalogUrlText"),
        snapshot.catalogUrlText
    ).toString();
    snapshot.deepSkyCatalogPresetIndex = readIntSetting(
        settings,
        SkyContextSettings::key("deepSkyCatalogPresetIndex"),
        snapshot.deepSkyCatalogPresetIndex
    );
    snapshot.deepSkyCatalogUrlText = settings.value(
        SkyContextSettings::key("deepSkyCatalogUrlText"),
        snapshot.deepSkyCatalogUrlText
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
    appendCachePath(cachePaths, configuredPath);
    appendCachePath(cachePaths, defaultPath);

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries = defaultDir.entryList(
            QStringList {"catalog-cache*.txt"},
            QDir::Files | QDir::Readable
        );
        for (const QString& entryName : matchingEntries) {
            appendCachePath(cachePaths, defaultDir.filePath(entryName));
        }
    }

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(SkyContextSettings::key("catalogCachePath"));
    settings.remove(SkyContextSettings::key("catalogSourceLabel"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLabelRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogConstellationCount"));
    settings.sync();
    return removedAllCacheFiles && settings.status() == QSettings::NoError;
}

bool SkySettingsStore::clearDeepSkyCatalogCache() const
{
    QSettings settings;
    QStringList cachePaths;
    appendCachePath(
        cachePaths,
        settings.value(
            SkyContextSettings::key("deepSkyCatalogCachePath"),
            SkyContextSettings::defaultDeepSkyCatalogCachePath()
        ).toString()
    );
    appendCachePath(cachePaths, SkyContextSettings::defaultDeepSkyCatalogCachePath());

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);
    settings.remove(SkyContextSettings::key("deepSkyCatalogCachePath"));
    settings.remove(SkyContextSettings::key("deepSkyCatalogSourceLabel"));
    settings.sync();
    return removedAllCacheFiles && settings.status() == QSettings::NoError;
}

bool SkySettingsStore::saveCatalogCache(const CatalogCacheSnapshot& snapshot) const
{
    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
    ).toString();
    const QString configuredDeepSkyPath = settings.value(
        SkyContextSettings::key("deepSkyCatalogCachePath"),
        SkyContextSettings::defaultDeepSkyCatalogCachePath()
    ).toString();
    const auto writePayload = [](const QString& path, const QByteArray& payload) {
        if (path.isEmpty() || payload.isEmpty()) {
            return true;
        }

        const QFileInfo targetInfo(path);
        QDir targetDir(targetInfo.absolutePath());
        if (!targetDir.mkpath(".")) {
            return false;
        }

        QSaveFile cacheFile(path);
        if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }

        const qint64 writtenBytes = cacheFile.write(payload);
        if (writtenBytes != payload.size()) {
            cacheFile.cancelWriting();
            return false;
        }

        return cacheFile.commit();
    };

    if (!writePayload(configuredPath, snapshot.catalogPayload)) {
        return false;
    }
    if (!writePayload(configuredDeepSkyPath, snapshot.deepSkyCatalogPayload)) {
        return false;
    }
    settings.setValue(
        SkyContextSettings::key("version"),
        SkyContextControllerConstants::kSettingsVersion
    );
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogCachePath"), configuredPath);
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(
            SkyContextSettings::key("deepSkyCatalogCachePath"),
            configuredDeepSkyPath
        );
    }
    if (!snapshot.catalogPayload.isEmpty()) {
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
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(
            SkyContextSettings::key("deepSkyCatalogSourceLabel"),
            snapshot.deepSkySourceLabel
        );
    }
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

    CatalogCacheSnapshot snapshot;
    QFile cacheFile(configuredPath);
    if (!configuredPath.isEmpty() && cacheFile.exists() && cacheFile.open(QIODevice::ReadOnly)) {
        snapshot.catalogPayload = cacheFile.readAll();
    }

    const QString configuredDeepSkyPath = settings.value(
        SkyContextSettings::key("deepSkyCatalogCachePath"),
        SkyContextSettings::defaultDeepSkyCatalogCachePath()
    ).toString();
    QFile deepSkyCacheFile(configuredDeepSkyPath);
    if (
        !configuredDeepSkyPath.isEmpty()
        && deepSkyCacheFile.exists()
        && deepSkyCacheFile.open(QIODevice::ReadOnly)
    ) {
        snapshot.deepSkyCatalogPayload = deepSkyCacheFile.readAll();
    }

    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return std::nullopt;
    }

    if (!snapshot.catalogPayload.isEmpty()) {
        snapshot.sourceLabel = settings.value(
            SkyContextSettings::key("catalogSourceLabel"),
            QString("Saved")
        ).toString();
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        snapshot.deepSkySourceLabel = settings.value(
            SkyContextSettings::key("deepSkyCatalogSourceLabel"),
            QString("Saved deep sky")
        ).toString();
    }
    snapshot.constellationLineRows = settings.value(
        SkyContextSettings::key("catalogConstellationLineRefs")
    ).toByteArray();
    snapshot.constellationLabelRows = settings.value(
        SkyContextSettings::key("catalogConstellationLabelRefs")
    ).toByteArray();
    snapshot.constellationLineSchemaVersion = readIntSetting(
        settings,
        SkyContextSettings::key("catalogConstellationLineSchemaVersion"),
        0
    );
    snapshot.constellationCount = static_cast<std::size_t>(
        readULongLongSetting(
            settings,
            SkyContextSettings::key("catalogConstellationCount"),
            static_cast<qulonglong>(0)
        )
    );
    return snapshot;
}
