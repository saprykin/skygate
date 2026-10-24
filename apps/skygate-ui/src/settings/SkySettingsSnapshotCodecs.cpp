#include "SkySettingsSnapshotCodecs.hpp"
#include "SkyCatalogSourceSettingsSnapshot.hpp"
#include "SkyEphemerisSettingsCodec.hpp"
#include "SkyLocationSettingsSnapshot.hpp"
#include "SkyLoggingSettingsSnapshot.hpp"
#include "SkySearchSettingsSnapshot.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "SkyStateSettingsSnapshot.hpp"
#include "SkyTimelineSettingsSnapshot.hpp"
#include "SkyViewSettingsSnapshot.hpp"
#include "diagnostics/SkyLogging.hpp"

#include <QSettings>

namespace skygate::ui::internal {
namespace {

void saveTimelineSettings(QSettings& settings, const SkyTimelineSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("live"), snapshot.live);
    settings.setValue(SkySettingsKeyBuilder::key("timelineToolbarCollapsed"), snapshot.toolbarCollapsed);
    settings.setValue(SkySettingsKeyBuilder::key("speedMultiplier"), snapshot.speedMultiplier);
    settings.setValue(SkySettingsKeyBuilder::key("stepSeconds"), snapshot.stepSeconds);
    settings.setValue(SkySettingsKeyBuilder::key("utcEpochMicros"), snapshot.utcEpochMicros);
}

SkyTimelineSettingsSnapshot loadTimelineSettings(QSettings& settings)
{
    SkyTimelineSettingsSnapshot snapshot;
    snapshot.live =
        SkySettingsValueCodecs::readBoolSetting(settings, SkySettingsKeyBuilder::key("live"), snapshot.live);
    snapshot.toolbarCollapsed = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("timelineToolbarCollapsed"), snapshot.toolbarCollapsed
    );
    snapshot.speedMultiplier = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("speedMultiplier"), snapshot.speedMultiplier
    );
    snapshot.stepSeconds = SkySettingsValueCodecs::readIntSetting(
        settings, SkySettingsKeyBuilder::key("stepSeconds"), snapshot.stepSeconds
    );
    snapshot.utcEpochMicros =
        settings.contains(SkySettingsKeyBuilder::key("utcEpochMicros"))
            ? SkySettingsValueCodecs::readLongLongSetting(
                  settings, SkySettingsKeyBuilder::key("utcEpochMicros"), snapshot.utcEpochMicros
              )
            : SkySettingsValueCodecs::readLongLongSetting(
                  settings, SkySettingsKeyBuilder::key("utcEpochSeconds"), snapshot.utcEpochMicros / 1'000'000
              ) * 1'000'000;
    return snapshot;
}

void saveSearchSettings(QSettings& settings, const SkySearchSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("searchToolbarCollapsed"), snapshot.toolbarCollapsed);
}

SkySearchSettingsSnapshot loadSearchSettings(QSettings& settings)
{
    SkySearchSettingsSnapshot snapshot;
    snapshot.toolbarCollapsed = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("searchToolbarCollapsed"), snapshot.toolbarCollapsed
    );
    return snapshot;
}

void saveViewSettings(QSettings& settings, const SkyViewSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("magnitudeCutoff"), snapshot.magnitudeCutoff);
    settings.setValue(SkySettingsKeyBuilder::key("viewCenterAltitudeDeg"), snapshot.centerAltitudeDeg);
    settings.setValue(SkySettingsKeyBuilder::key("viewCenterAzimuthDeg"), snapshot.centerAzimuthDeg);
    settings.setValue(SkySettingsKeyBuilder::key("viewFieldOfViewDeg"), snapshot.fieldOfViewDeg);
    settings.setValue(SkySettingsKeyBuilder::key("projectionType"), snapshot.projectionTypeText);
    settings.setValue(SkySettingsKeyBuilder::key("themeId"), snapshot.themeId);
}

SkyViewSettingsSnapshot loadViewSettings(QSettings& settings)
{
    SkyViewSettingsSnapshot snapshot;
    snapshot.magnitudeCutoff = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("magnitudeCutoff"), snapshot.magnitudeCutoff
    );
    snapshot.centerAltitudeDeg = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("viewCenterAltitudeDeg"), snapshot.centerAltitudeDeg
    );
    snapshot.centerAzimuthDeg = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("viewCenterAzimuthDeg"), snapshot.centerAzimuthDeg
    );
    snapshot.fieldOfViewDeg = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("viewFieldOfViewDeg"), snapshot.fieldOfViewDeg
    );
    snapshot.projectionTypeText =
        settings.value(SkySettingsKeyBuilder::key("projectionType"), snapshot.projectionTypeText).toString();
    snapshot.themeId = settings.value(SkySettingsKeyBuilder::key("themeId"), snapshot.themeId).toString();
    return snapshot;
}

void saveLocationSettings(QSettings& settings, const SkyLocationSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("latitudeDeg"), snapshot.latitudeDeg);
    settings.setValue(SkySettingsKeyBuilder::key("longitudeDeg"), snapshot.longitudeDeg);
    settings.setValue(SkySettingsKeyBuilder::key("elevationMeters"), snapshot.elevationMeters);
    settings.setValue(SkySettingsKeyBuilder::key("locationSource"), snapshot.sourceText);
    settings.setValue(SkySettingsKeyBuilder::key("selectedCityId"), snapshot.selectedCityId);
    settings.setValue(SkySettingsKeyBuilder::key("displayTimeZoneId"), snapshot.displayTimeZoneId);
}

SkyLocationSettingsSnapshot loadLocationSettings(QSettings& settings)
{
    SkyLocationSettingsSnapshot snapshot;
    snapshot.latitudeDeg = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("latitudeDeg"), snapshot.latitudeDeg
    );
    snapshot.longitudeDeg = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("longitudeDeg"), snapshot.longitudeDeg
    );
    snapshot.elevationMeters = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key("elevationMeters"), snapshot.elevationMeters
    );
    snapshot.sourceText = settings.value(SkySettingsKeyBuilder::key("locationSource"), snapshot.sourceText).toString();
    snapshot.selectedCityId =
        settings.value(SkySettingsKeyBuilder::key("selectedCityId"), snapshot.selectedCityId).toString();
    snapshot.displayTimeZoneId =
        settings.value(SkySettingsKeyBuilder::key("displayTimeZoneId"), snapshot.displayTimeZoneId).toString();
    return snapshot;
}

void saveOverlaySettings(QSettings& settings, const SkyOverlayLayerVisibility& visibility)
{
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/horizon"), visibility.horizon);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/altAzGrid"), visibility.altAzGrid);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/constellationLines"), visibility.constellationLines);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/constellationLabels"), visibility.constellationLabels);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/ecliptic"), visibility.ecliptic);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/celestialEquator"), visibility.celestialEquator);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/circumpolarBoundary"), visibility.circumpolarBoundary);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/solarSystemLabels"), visibility.solarSystemLabels);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/deepSkyObjects"), visibility.deepSkyObjects);
    settings.setValue(SkySettingsKeyBuilder::key("overlayLayers/deepSkyLabels"), visibility.deepSkyLabels);
}

SkyOverlayLayerVisibility loadOverlaySettings(QSettings& settings)
{
    SkyOverlayLayerVisibility visibility;
    visibility.horizon = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/horizon"), visibility.horizon
    );
    visibility.altAzGrid = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/altAzGrid"), visibility.altAzGrid
    );
    visibility.constellationLines = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/constellationLines"), visibility.constellationLines
    );
    visibility.constellationLabels = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/constellationLabels"), visibility.constellationLabels
    );
    visibility.ecliptic = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/ecliptic"), visibility.ecliptic
    );
    visibility.celestialEquator = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/celestialEquator"), visibility.celestialEquator
    );
    visibility.circumpolarBoundary = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/circumpolarBoundary"), visibility.circumpolarBoundary
    );
    visibility.solarSystemLabels = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/solarSystemLabels"), visibility.solarSystemLabels
    );
    visibility.deepSkyObjects = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/deepSkyObjects"), visibility.deepSkyObjects
    );
    visibility.deepSkyLabels = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("overlayLayers/deepSkyLabels"), visibility.deepSkyLabels
    );
    return visibility;
}

void saveCatalogSourceSettings(QSettings& settings, const SkyCatalogSourceSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("catalogPresetIndex"), snapshot.starPresetIndex);
    settings.setValue(SkySettingsKeyBuilder::key("catalogUrlText"), snapshot.starUrlText);
    settings.setValue(SkySettingsKeyBuilder::key("deepSkyCatalogPresetIndex"), snapshot.deepSkyPresetIndex);
    settings.setValue(SkySettingsKeyBuilder::key("deepSkyCatalogUrlText"), snapshot.deepSkyUrlText);
}

SkyCatalogSourceSettingsSnapshot loadCatalogSourceSettings(QSettings& settings)
{
    SkyCatalogSourceSettingsSnapshot snapshot;
    snapshot.starPresetIndex = SkySettingsValueCodecs::readIntSetting(
        settings, SkySettingsKeyBuilder::key("catalogPresetIndex"), snapshot.starPresetIndex
    );
    snapshot.starUrlText =
        settings.value(SkySettingsKeyBuilder::key("catalogUrlText"), snapshot.starUrlText).toString();
    snapshot.deepSkyPresetIndex = SkySettingsValueCodecs::readIntSetting(
        settings, SkySettingsKeyBuilder::key("deepSkyCatalogPresetIndex"), snapshot.deepSkyPresetIndex
    );
    snapshot.deepSkyUrlText =
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogUrlText"), snapshot.deepSkyUrlText).toString();
    return snapshot;
}

void saveLoggingSettings(QSettings& settings, const SkyLoggingSettingsSnapshot& snapshot)
{
    settings.setValue(SkySettingsKeyBuilder::key("logging/logToTerminal"), snapshot.logToTerminal);
    settings.setValue(SkySettingsKeyBuilder::key("logging/logToFile"), snapshot.logToFile);
    SkySettingsValueCodecs::writeNonBlankStringSetting(
        settings,
        SkySettingsKeyBuilder::key("logging/logFilePath"),
        snapshot.logFilePath,
        skygate::ui::SkyLogging::defaultLogFilePath()
    );
}

SkyLoggingSettingsSnapshot readLoggingSettings(QSettings& settings)
{
    SkyLoggingSettingsSnapshot snapshot;
    snapshot.logToTerminal = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("logging/logToTerminal"), snapshot.logToTerminal
    );
    snapshot.logToFile = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key("logging/logToFile"), snapshot.logToFile
    );
    snapshot.logFilePath = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, SkySettingsKeyBuilder::key("logging/logFilePath"), skygate::ui::SkyLogging::defaultLogFilePath()
    );
    return snapshot;
}

}  // namespace

SkyLoggingSettingsSnapshot SkySettingsSnapshotCodecs::loadLoggingSettings(QSettings& settings)
{
    return readLoggingSettings(settings);
}

SkyStateSettingsSnapshot SkySettingsSnapshotCodecs::splitStateSnapshot(const SkyPersistedStateSnapshot& snapshot)
{
    SkyStateSettingsSnapshot domains;
    domains.timeline.live = snapshot.live;
    domains.timeline.toolbarCollapsed = snapshot.timelineToolbarCollapsed;
    domains.timeline.speedMultiplier = snapshot.speedMultiplier;
    domains.timeline.stepSeconds = snapshot.stepSeconds;
    domains.timeline.utcEpochMicros = snapshot.utcEpochMicros;
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
    domains.ephemeris = snapshot.ephemeris;
    domains.ephemerisSettingsPresent = snapshot.ephemerisSettingsPresent;
    return domains;
}

SkyPersistedStateSnapshot SkySettingsSnapshotCodecs::mergeStateSnapshot(const SkyStateSettingsSnapshot& domains)
{
    SkyPersistedStateSnapshot snapshot;
    snapshot.live = domains.timeline.live;
    snapshot.timelineToolbarCollapsed = domains.timeline.toolbarCollapsed;
    snapshot.searchToolbarCollapsed = domains.search.toolbarCollapsed;
    snapshot.speedMultiplier = domains.timeline.speedMultiplier;
    snapshot.stepSeconds = domains.timeline.stepSeconds;
    snapshot.magnitudeCutoff = domains.view.magnitudeCutoff;
    snapshot.viewCenterAltitudeDeg = domains.view.centerAltitudeDeg;
    snapshot.viewCenterAzimuthDeg = domains.view.centerAzimuthDeg;
    snapshot.viewFieldOfViewDeg = domains.view.fieldOfViewDeg;
    snapshot.utcEpochMicros = domains.timeline.utcEpochMicros;
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
    snapshot.ephemeris = domains.ephemeris;
    snapshot.ephemerisSettingsPresent = domains.ephemerisSettingsPresent;
    return snapshot;
}

void SkySettingsSnapshotCodecs::saveStateSnapshot(QSettings& settings, const SkyPersistedStateSnapshot& snapshot)
{
    const SkyStateSettingsSnapshot domains = splitStateSnapshot(snapshot);
    settings.setValue(SkySettingsKeyBuilder::key("version"), SkyPersistedStateSnapshot::kSettingsVersion);
    saveTimelineSettings(settings, domains.timeline);
    saveSearchSettings(settings, domains.search);
    saveViewSettings(settings, domains.view);
    saveLocationSettings(settings, domains.location);
    saveOverlaySettings(settings, domains.overlayLayers);
    saveCatalogSourceSettings(settings, domains.catalogSources);
    saveLoggingSettings(settings, domains.logging);
    SkyEphemerisSettingsCodec::save(settings, domains.ephemeris);
}

std::optional<SkyPersistedStateSnapshot> SkySettingsSnapshotCodecs::loadStateSnapshot(QSettings& settings)
{
    if (!settings.contains(SkySettingsKeyBuilder::key("version"))) {
        return std::nullopt;
    }

    SkyStateSettingsSnapshot domains;
    domains.timeline = loadTimelineSettings(settings);
    domains.search = loadSearchSettings(settings);
    domains.view = loadViewSettings(settings);
    domains.location = loadLocationSettings(settings);
    domains.overlayLayers = loadOverlaySettings(settings);
    domains.catalogSources = loadCatalogSourceSettings(settings);
    domains.logging = SkySettingsSnapshotCodecs::loadLoggingSettings(settings);
    domains.ephemerisSettingsPresent = SkyEphemerisSettingsCodec::hasStoredSettings(settings);
    if (domains.ephemerisSettingsPresent) {
        domains.ephemeris = SkyEphemerisSettingsCodec::load(settings);
    }
    return mergeStateSnapshot(domains);
}

}  // namespace skygate::ui::internal
