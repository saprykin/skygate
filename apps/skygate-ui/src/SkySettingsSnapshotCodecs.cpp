#include "SkySettingsSnapshotCodecs.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyLogging.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QSettings>

namespace skygate::ui::internal {
namespace {

QString settingsKey(const char* name)
{
    return SkyContextSettings::key(QString::fromUtf8(name));
}

void saveTimelineSettings(QSettings& settings, const SkyTimelineSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("live"), snapshot.live);
    settings.setValue(settingsKey("timelineToolbarCollapsed"), snapshot.toolbarCollapsed);
    settings.setValue(settingsKey("speedMultiplier"), snapshot.speedMultiplier);
    settings.setValue(settingsKey("stepSeconds"), snapshot.stepSeconds);
    settings.setValue(settingsKey("utcEpochSeconds"), snapshot.utcEpochSeconds);
}

SkyTimelineSettingsSnapshot loadTimelineSettings(QSettings& settings)
{
    SkyTimelineSettingsSnapshot snapshot;
    snapshot.live = readBoolSetting(settings, settingsKey("live"), snapshot.live);
    snapshot.toolbarCollapsed = readBoolSetting(
        settings,
        settingsKey("timelineToolbarCollapsed"),
        snapshot.toolbarCollapsed
    );
    snapshot.speedMultiplier = readDoubleSetting(
        settings,
        settingsKey("speedMultiplier"),
        snapshot.speedMultiplier
    );
    snapshot.stepSeconds = readIntSetting(
        settings,
        settingsKey("stepSeconds"),
        snapshot.stepSeconds
    );
    snapshot.utcEpochSeconds = readLongLongSetting(
        settings,
        settingsKey("utcEpochSeconds"),
        snapshot.utcEpochSeconds
    );
    return snapshot;
}

void saveSearchSettings(QSettings& settings, const SkySearchSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("searchToolbarCollapsed"), snapshot.toolbarCollapsed);
}

SkySearchSettingsSnapshot loadSearchSettings(QSettings& settings)
{
    SkySearchSettingsSnapshot snapshot;
    snapshot.toolbarCollapsed = readBoolSetting(
        settings,
        settingsKey("searchToolbarCollapsed"),
        snapshot.toolbarCollapsed
    );
    return snapshot;
}

void saveViewSettings(QSettings& settings, const SkyViewSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("magnitudeCutoff"), snapshot.magnitudeCutoff);
    settings.setValue(settingsKey("viewCenterAltitudeDeg"), snapshot.centerAltitudeDeg);
    settings.setValue(settingsKey("viewCenterAzimuthDeg"), snapshot.centerAzimuthDeg);
    settings.setValue(settingsKey("viewFieldOfViewDeg"), snapshot.fieldOfViewDeg);
    settings.setValue(settingsKey("projectionType"), snapshot.projectionTypeText);
    settings.setValue(settingsKey("themeId"), snapshot.themeId);
}

SkyViewSettingsSnapshot loadViewSettings(QSettings& settings)
{
    SkyViewSettingsSnapshot snapshot;
    snapshot.magnitudeCutoff = readDoubleSetting(
        settings,
        settingsKey("magnitudeCutoff"),
        snapshot.magnitudeCutoff
    );
    snapshot.centerAltitudeDeg = readDoubleSetting(
        settings,
        settingsKey("viewCenterAltitudeDeg"),
        snapshot.centerAltitudeDeg
    );
    snapshot.centerAzimuthDeg = readDoubleSetting(
        settings,
        settingsKey("viewCenterAzimuthDeg"),
        snapshot.centerAzimuthDeg
    );
    snapshot.fieldOfViewDeg = readDoubleSetting(
        settings,
        settingsKey("viewFieldOfViewDeg"),
        snapshot.fieldOfViewDeg
    );
    snapshot.projectionTypeText = settings.value(
        settingsKey("projectionType"),
        snapshot.projectionTypeText
    ).toString();
    snapshot.themeId = settings.value(settingsKey("themeId"), snapshot.themeId).toString();
    return snapshot;
}

void saveLocationSettings(QSettings& settings, const SkyLocationSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("latitudeDeg"), snapshot.latitudeDeg);
    settings.setValue(settingsKey("longitudeDeg"), snapshot.longitudeDeg);
    settings.setValue(settingsKey("elevationMeters"), snapshot.elevationMeters);
    settings.setValue(settingsKey("locationSource"), snapshot.sourceText);
    settings.setValue(settingsKey("selectedCityId"), snapshot.selectedCityId);
    settings.setValue(settingsKey("displayTimeZoneId"), snapshot.displayTimeZoneId);
}

SkyLocationSettingsSnapshot loadLocationSettings(QSettings& settings)
{
    SkyLocationSettingsSnapshot snapshot;
    snapshot.latitudeDeg = readDoubleSetting(
        settings,
        settingsKey("latitudeDeg"),
        snapshot.latitudeDeg
    );
    snapshot.longitudeDeg = readDoubleSetting(
        settings,
        settingsKey("longitudeDeg"),
        snapshot.longitudeDeg
    );
    snapshot.elevationMeters = readDoubleSetting(
        settings,
        settingsKey("elevationMeters"),
        snapshot.elevationMeters
    );
    snapshot.sourceText = settings.value(
        settingsKey("locationSource"),
        snapshot.sourceText
    ).toString();
    snapshot.selectedCityId = settings.value(
        settingsKey("selectedCityId"),
        snapshot.selectedCityId
    ).toString();
    snapshot.displayTimeZoneId = settings.value(
        settingsKey("displayTimeZoneId"),
        snapshot.displayTimeZoneId
    ).toString();
    return snapshot;
}

void saveOverlaySettings(QSettings& settings, const SkyOverlayLayerVisibility& visibility)
{
    settings.setValue(settingsKey("overlayLayers/horizon"), visibility.horizon);
    settings.setValue(settingsKey("overlayLayers/altAzGrid"), visibility.altAzGrid);
    settings.setValue(
        settingsKey("overlayLayers/constellationLines"),
        visibility.constellationLines
    );
    settings.setValue(
        settingsKey("overlayLayers/constellationLabels"),
        visibility.constellationLabels
    );
    settings.setValue(settingsKey("overlayLayers/ecliptic"), visibility.ecliptic);
    settings.setValue(
        settingsKey("overlayLayers/celestialEquator"),
        visibility.celestialEquator
    );
    settings.setValue(
        settingsKey("overlayLayers/circumpolarBoundary"),
        visibility.circumpolarBoundary
    );
    settings.setValue(
        settingsKey("overlayLayers/solarSystemLabels"),
        visibility.solarSystemLabels
    );
    settings.setValue(settingsKey("overlayLayers/deepSkyObjects"), visibility.deepSkyObjects);
    settings.setValue(settingsKey("overlayLayers/deepSkyLabels"), visibility.deepSkyLabels);
}

SkyOverlayLayerVisibility loadOverlaySettings(QSettings& settings)
{
    SkyOverlayLayerVisibility visibility;
    visibility.horizon = readBoolSetting(
        settings,
        settingsKey("overlayLayers/horizon"),
        visibility.horizon
    );
    visibility.altAzGrid = readBoolSetting(
        settings,
        settingsKey("overlayLayers/altAzGrid"),
        visibility.altAzGrid
    );
    visibility.constellationLines = readBoolSetting(
        settings,
        settingsKey("overlayLayers/constellationLines"),
        visibility.constellationLines
    );
    visibility.constellationLabels = readBoolSetting(
        settings,
        settingsKey("overlayLayers/constellationLabels"),
        visibility.constellationLabels
    );
    visibility.ecliptic = readBoolSetting(
        settings,
        settingsKey("overlayLayers/ecliptic"),
        visibility.ecliptic
    );
    visibility.celestialEquator = readBoolSetting(
        settings,
        settingsKey("overlayLayers/celestialEquator"),
        visibility.celestialEquator
    );
    visibility.circumpolarBoundary = readBoolSetting(
        settings,
        settingsKey("overlayLayers/circumpolarBoundary"),
        visibility.circumpolarBoundary
    );
    visibility.solarSystemLabels = readBoolSetting(
        settings,
        settingsKey("overlayLayers/solarSystemLabels"),
        visibility.solarSystemLabels
    );
    visibility.deepSkyObjects = readBoolSetting(
        settings,
        settingsKey("overlayLayers/deepSkyObjects"),
        visibility.deepSkyObjects
    );
    visibility.deepSkyLabels = readBoolSetting(
        settings,
        settingsKey("overlayLayers/deepSkyLabels"),
        visibility.deepSkyLabels
    );
    return visibility;
}

void saveCatalogSourceSettings(
    QSettings& settings,
    const SkyCatalogSourceSettingsSnapshot& snapshot
)
{
    settings.setValue(settingsKey("catalogPresetIndex"), snapshot.starPresetIndex);
    settings.setValue(settingsKey("catalogUrlText"), snapshot.starUrlText);
    settings.setValue(settingsKey("deepSkyCatalogPresetIndex"), snapshot.deepSkyPresetIndex);
    settings.setValue(settingsKey("deepSkyCatalogUrlText"), snapshot.deepSkyUrlText);
}

SkyCatalogSourceSettingsSnapshot loadCatalogSourceSettings(QSettings& settings)
{
    SkyCatalogSourceSettingsSnapshot snapshot;
    snapshot.starPresetIndex = readIntSetting(
        settings,
        settingsKey("catalogPresetIndex"),
        snapshot.starPresetIndex
    );
    snapshot.starUrlText = settings.value(
        settingsKey("catalogUrlText"),
        snapshot.starUrlText
    ).toString();
    snapshot.deepSkyPresetIndex = readIntSetting(
        settings,
        settingsKey("deepSkyCatalogPresetIndex"),
        snapshot.deepSkyPresetIndex
    );
    snapshot.deepSkyUrlText = settings.value(
        settingsKey("deepSkyCatalogUrlText"),
        snapshot.deepSkyUrlText
    ).toString();
    return snapshot;
}

void saveLoggingSettings(QSettings& settings, const SkyLoggingSettingsSnapshot& snapshot)
{
    const QString logFilePath = snapshot.logFilePath.trimmed().isEmpty()
        ? skygate::ui::SkyLogging::defaultLogFilePath()
        : snapshot.logFilePath.trimmed();

    settings.setValue(settingsKey("logging/logToTerminal"), snapshot.logToTerminal);
    settings.setValue(settingsKey("logging/logToFile"), snapshot.logToFile);
    settings.setValue(settingsKey("logging/logFilePath"), logFilePath);
}

SkyLoggingSettingsSnapshot loadLoggingSettings(QSettings& settings)
{
    SkyLoggingSettingsSnapshot snapshot;
    snapshot.logToTerminal = readBoolSetting(
        settings,
        settingsKey("logging/logToTerminal"),
        snapshot.logToTerminal
    );
    snapshot.logToFile = readBoolSetting(
        settings,
        settingsKey("logging/logToFile"),
        snapshot.logToFile
    );
    snapshot.logFilePath = readNonBlankStringSetting(
        settings,
        settingsKey("logging/logFilePath"),
        skygate::ui::SkyLogging::defaultLogFilePath()
    );
    return snapshot;
}

}  // namespace

SkyStateSettingsSnapshot splitStateSnapshot(const SkySettingsStore::StateSnapshot& snapshot)
{
    SkyStateSettingsSnapshot domains;
    domains.timeline.live = snapshot.live;
    domains.timeline.toolbarCollapsed = snapshot.timelineToolbarCollapsed;
    domains.timeline.speedMultiplier = snapshot.speedMultiplier;
    domains.timeline.stepSeconds = snapshot.stepSeconds;
    domains.timeline.utcEpochSeconds = snapshot.utcEpochSeconds;
    domains.search.toolbarCollapsed = snapshot.searchToolbarCollapsed;
    domains.view.magnitudeCutoff = snapshot.magnitudeCutoff;
    domains.view.centerAltitudeDeg = snapshot.viewCenterAltitudeDeg;
    domains.view.centerAzimuthDeg = snapshot.viewCenterAzimuthDeg;
    domains.view.fieldOfViewDeg = snapshot.viewFieldOfViewDeg;
    domains.view.projectionTypeText = snapshot.projectionTypeText;
    domains.view.themeId = snapshot.themeId;
    domains.location.latitudeDeg = snapshot.latitudeDeg;
    domains.location.longitudeDeg = snapshot.longitudeDeg;
    domains.location.elevationMeters = snapshot.elevationMeters;
    domains.location.sourceText = snapshot.locationSourceText;
    domains.location.selectedCityId = snapshot.selectedCityId;
    domains.location.displayTimeZoneId = snapshot.displayTimeZoneId;
    domains.overlayLayers = snapshot.overlayLayers;
    domains.catalogSources.starPresetIndex = snapshot.catalogPresetIndex;
    domains.catalogSources.starUrlText = snapshot.catalogUrlText;
    domains.catalogSources.deepSkyPresetIndex = snapshot.deepSkyCatalogPresetIndex;
    domains.catalogSources.deepSkyUrlText = snapshot.deepSkyCatalogUrlText;
    domains.logging.logToTerminal = snapshot.logToTerminal;
    domains.logging.logToFile = snapshot.logToFile;
    domains.logging.logFilePath = snapshot.logFilePath;
    return domains;
}

SkySettingsStore::StateSnapshot mergeStateSnapshot(const SkyStateSettingsSnapshot& domains)
{
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = domains.timeline.live;
    snapshot.timelineToolbarCollapsed = domains.timeline.toolbarCollapsed;
    snapshot.searchToolbarCollapsed = domains.search.toolbarCollapsed;
    snapshot.speedMultiplier = domains.timeline.speedMultiplier;
    snapshot.stepSeconds = domains.timeline.stepSeconds;
    snapshot.magnitudeCutoff = domains.view.magnitudeCutoff;
    snapshot.viewCenterAltitudeDeg = domains.view.centerAltitudeDeg;
    snapshot.viewCenterAzimuthDeg = domains.view.centerAzimuthDeg;
    snapshot.viewFieldOfViewDeg = domains.view.fieldOfViewDeg;
    snapshot.utcEpochSeconds = domains.timeline.utcEpochSeconds;
    snapshot.latitudeDeg = domains.location.latitudeDeg;
    snapshot.longitudeDeg = domains.location.longitudeDeg;
    snapshot.elevationMeters = domains.location.elevationMeters;
    snapshot.locationSourceText = domains.location.sourceText;
    snapshot.selectedCityId = domains.location.selectedCityId;
    snapshot.displayTimeZoneId = domains.location.displayTimeZoneId;
    snapshot.projectionTypeText = domains.view.projectionTypeText;
    snapshot.themeId = domains.view.themeId;
    snapshot.overlayLayers = domains.overlayLayers;
    snapshot.catalogPresetIndex = domains.catalogSources.starPresetIndex;
    snapshot.catalogUrlText = domains.catalogSources.starUrlText;
    snapshot.deepSkyCatalogPresetIndex = domains.catalogSources.deepSkyPresetIndex;
    snapshot.deepSkyCatalogUrlText = domains.catalogSources.deepSkyUrlText;
    snapshot.logToTerminal = domains.logging.logToTerminal;
    snapshot.logToFile = domains.logging.logToFile;
    snapshot.logFilePath = domains.logging.logFilePath;
    return snapshot;
}

void saveStateSnapshot(QSettings& settings, const SkySettingsStore::StateSnapshot& snapshot)
{
    const SkyStateSettingsSnapshot domains = splitStateSnapshot(snapshot);
    settings.setValue(settingsKey("version"), SkyContextControllerConstants::kSettingsVersion);
    saveTimelineSettings(settings, domains.timeline);
    saveSearchSettings(settings, domains.search);
    saveViewSettings(settings, domains.view);
    saveLocationSettings(settings, domains.location);
    saveOverlaySettings(settings, domains.overlayLayers);
    saveCatalogSourceSettings(settings, domains.catalogSources);
    saveLoggingSettings(settings, domains.logging);
}

std::optional<SkySettingsStore::StateSnapshot> loadStateSnapshot(QSettings& settings)
{
    if (!settings.contains(settingsKey("version"))) {
        return std::nullopt;
    }

    SkyStateSettingsSnapshot domains;
    domains.timeline = loadTimelineSettings(settings);
    domains.search = loadSearchSettings(settings);
    domains.view = loadViewSettings(settings);
    domains.location = loadLocationSettings(settings);
    domains.overlayLayers = loadOverlaySettings(settings);
    domains.catalogSources = loadCatalogSourceSettings(settings);
    domains.logging = loadLoggingSettings(settings);
    return mergeStateSnapshot(domains);
}

}  // namespace skygate::ui::internal
