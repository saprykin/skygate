#include "SkySettingsSnapshotCodecs.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyLogging.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QSettings>

#include <cstdint>

namespace skygate::ui::internal {
namespace {

using skygate::ephemeris::EphemerisCorrectionFlags;
using skygate::ephemeris::EphemerisEngineKind;

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
    settings.setValue(settingsKey("utcEpochMicros"), snapshot.utcEpochMicros);
}

SkyTimelineSettingsSnapshot loadTimelineSettings(QSettings& settings)
{
    SkyTimelineSettingsSnapshot snapshot;
    snapshot.live = readBoolSetting(settings, settingsKey("live"), snapshot.live);
    snapshot.toolbarCollapsed =
        readBoolSetting(settings, settingsKey("timelineToolbarCollapsed"), snapshot.toolbarCollapsed);
    snapshot.speedMultiplier = readDoubleSetting(settings, settingsKey("speedMultiplier"), snapshot.speedMultiplier);
    snapshot.stepSeconds = readIntSetting(settings, settingsKey("stepSeconds"), snapshot.stepSeconds);
    snapshot.utcEpochMicros =
        settings.contains(settingsKey("utcEpochMicros"))
            ? readLongLongSetting(settings, settingsKey("utcEpochMicros"), snapshot.utcEpochMicros)
            : readLongLongSetting(settings, settingsKey("utcEpochSeconds"), snapshot.utcEpochMicros / 1'000'000)
                  * 1'000'000;
    return snapshot;
}

void saveSearchSettings(QSettings& settings, const SkySearchSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("searchToolbarCollapsed"), snapshot.toolbarCollapsed);
}

SkySearchSettingsSnapshot loadSearchSettings(QSettings& settings)
{
    SkySearchSettingsSnapshot snapshot;
    snapshot.toolbarCollapsed =
        readBoolSetting(settings, settingsKey("searchToolbarCollapsed"), snapshot.toolbarCollapsed);
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
    snapshot.magnitudeCutoff = readDoubleSetting(settings, settingsKey("magnitudeCutoff"), snapshot.magnitudeCutoff);
    snapshot.centerAltitudeDeg =
        readDoubleSetting(settings, settingsKey("viewCenterAltitudeDeg"), snapshot.centerAltitudeDeg);
    snapshot.centerAzimuthDeg =
        readDoubleSetting(settings, settingsKey("viewCenterAzimuthDeg"), snapshot.centerAzimuthDeg);
    snapshot.fieldOfViewDeg = readDoubleSetting(settings, settingsKey("viewFieldOfViewDeg"), snapshot.fieldOfViewDeg);
    snapshot.projectionTypeText = settings.value(settingsKey("projectionType"), snapshot.projectionTypeText).toString();
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
    snapshot.latitudeDeg = readDoubleSetting(settings, settingsKey("latitudeDeg"), snapshot.latitudeDeg);
    snapshot.longitudeDeg = readDoubleSetting(settings, settingsKey("longitudeDeg"), snapshot.longitudeDeg);
    snapshot.elevationMeters = readDoubleSetting(settings, settingsKey("elevationMeters"), snapshot.elevationMeters);
    snapshot.sourceText = settings.value(settingsKey("locationSource"), snapshot.sourceText).toString();
    snapshot.selectedCityId = settings.value(settingsKey("selectedCityId"), snapshot.selectedCityId).toString();
    snapshot.displayTimeZoneId =
        settings.value(settingsKey("displayTimeZoneId"), snapshot.displayTimeZoneId).toString();
    return snapshot;
}

void saveOverlaySettings(QSettings& settings, const SkyOverlayLayerVisibility& visibility)
{
    settings.setValue(settingsKey("overlayLayers/horizon"), visibility.horizon);
    settings.setValue(settingsKey("overlayLayers/altAzGrid"), visibility.altAzGrid);
    settings.setValue(settingsKey("overlayLayers/constellationLines"), visibility.constellationLines);
    settings.setValue(settingsKey("overlayLayers/constellationLabels"), visibility.constellationLabels);
    settings.setValue(settingsKey("overlayLayers/ecliptic"), visibility.ecliptic);
    settings.setValue(settingsKey("overlayLayers/celestialEquator"), visibility.celestialEquator);
    settings.setValue(settingsKey("overlayLayers/circumpolarBoundary"), visibility.circumpolarBoundary);
    settings.setValue(settingsKey("overlayLayers/solarSystemLabels"), visibility.solarSystemLabels);
    settings.setValue(settingsKey("overlayLayers/deepSkyObjects"), visibility.deepSkyObjects);
    settings.setValue(settingsKey("overlayLayers/deepSkyLabels"), visibility.deepSkyLabels);
}

SkyOverlayLayerVisibility loadOverlaySettings(QSettings& settings)
{
    SkyOverlayLayerVisibility visibility;
    visibility.horizon = readBoolSetting(settings, settingsKey("overlayLayers/horizon"), visibility.horizon);
    visibility.altAzGrid = readBoolSetting(settings, settingsKey("overlayLayers/altAzGrid"), visibility.altAzGrid);
    visibility.constellationLines =
        readBoolSetting(settings, settingsKey("overlayLayers/constellationLines"), visibility.constellationLines);
    visibility.constellationLabels =
        readBoolSetting(settings, settingsKey("overlayLayers/constellationLabels"), visibility.constellationLabels);
    visibility.ecliptic = readBoolSetting(settings, settingsKey("overlayLayers/ecliptic"), visibility.ecliptic);
    visibility.celestialEquator =
        readBoolSetting(settings, settingsKey("overlayLayers/celestialEquator"), visibility.celestialEquator);
    visibility.circumpolarBoundary =
        readBoolSetting(settings, settingsKey("overlayLayers/circumpolarBoundary"), visibility.circumpolarBoundary);
    visibility.solarSystemLabels =
        readBoolSetting(settings, settingsKey("overlayLayers/solarSystemLabels"), visibility.solarSystemLabels);
    visibility.deepSkyObjects =
        readBoolSetting(settings, settingsKey("overlayLayers/deepSkyObjects"), visibility.deepSkyObjects);
    visibility.deepSkyLabels =
        readBoolSetting(settings, settingsKey("overlayLayers/deepSkyLabels"), visibility.deepSkyLabels);
    return visibility;
}

void saveCatalogSourceSettings(QSettings& settings, const SkyCatalogSourceSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("catalogPresetIndex"), snapshot.starPresetIndex);
    settings.setValue(settingsKey("catalogUrlText"), snapshot.starUrlText);
    settings.setValue(settingsKey("deepSkyCatalogPresetIndex"), snapshot.deepSkyPresetIndex);
    settings.setValue(settingsKey("deepSkyCatalogUrlText"), snapshot.deepSkyUrlText);
}

SkyCatalogSourceSettingsSnapshot loadCatalogSourceSettings(QSettings& settings)
{
    SkyCatalogSourceSettingsSnapshot snapshot;
    snapshot.starPresetIndex = readIntSetting(settings, settingsKey("catalogPresetIndex"), snapshot.starPresetIndex);
    snapshot.starUrlText = settings.value(settingsKey("catalogUrlText"), snapshot.starUrlText).toString();
    snapshot.deepSkyPresetIndex =
        readIntSetting(settings, settingsKey("deepSkyCatalogPresetIndex"), snapshot.deepSkyPresetIndex);
    snapshot.deepSkyUrlText = settings.value(settingsKey("deepSkyCatalogUrlText"), snapshot.deepSkyUrlText).toString();
    return snapshot;
}

void saveLoggingSettings(QSettings& settings, const SkyLoggingSettingsSnapshot& snapshot)
{
    const QString logFilePath = snapshot.logFilePath.trimmed().isEmpty() ? skygate::ui::SkyLogging::defaultLogFilePath()
                                                                         : snapshot.logFilePath.trimmed();

    settings.setValue(settingsKey("logging/logToTerminal"), snapshot.logToTerminal);
    settings.setValue(settingsKey("logging/logToFile"), snapshot.logToFile);
    settings.setValue(settingsKey("logging/logFilePath"), logFilePath);
}

SkyLoggingSettingsSnapshot loadLoggingSettings(QSettings& settings)
{
    SkyLoggingSettingsSnapshot snapshot;
    snapshot.logToTerminal = readBoolSetting(settings, settingsKey("logging/logToTerminal"), snapshot.logToTerminal);
    snapshot.logToFile = readBoolSetting(settings, settingsKey("logging/logToFile"), snapshot.logToFile);
    snapshot.logFilePath = readNonBlankStringSetting(
        settings, settingsKey("logging/logFilePath"), skygate::ui::SkyLogging::defaultLogFilePath()
    );
    return snapshot;
}

[[nodiscard]] QString ephemerisEngineKindToString(const EphemerisEngineKind::Type engineKind)
{
    switch (engineKind) {
    case EphemerisEngineKind::Type::Simple:
        return QStringLiteral("simple");
    case EphemerisEngineKind::Type::HighPrecision:
        return QStringLiteral("highPrecision");
    case EphemerisEngineKind::Type::Last:
        break;
    }
    return QStringLiteral("simple");
}

[[nodiscard]] EphemerisEngineKind::Type
ephemerisEngineKindFromString(const QString& text, const EphemerisEngineKind::Type fallback)
{
    const QString normalizedText = text.trimmed().toLower();
    if (normalizedText == QStringLiteral("simple")) {
        return EphemerisEngineKind::Type::Simple;
    }
    if (normalizedText == QStringLiteral("highprecision") || normalizedText == QStringLiteral("high-precision")) {
        return EphemerisEngineKind::Type::HighPrecision;
    }
    return fallback;
}

[[nodiscard]] QString normalizedNonBlankSetting(QSettings& settings, const QString& key, const QString& fallback)
{
    const QString value = settings.value(key, fallback).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

[[nodiscard]] EphemerisCorrectionFlags
readCorrectionFlags(QSettings& settings, const QString& key, const EphemerisCorrectionFlags fallback)
{
    constexpr std::uint32_t kSupportedCorrectionMask =
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::apparentTopocentric());
    const qulonglong rawFlags =
        readULongLongSetting(settings, key, static_cast<qulonglong>(static_cast<std::uint32_t>(fallback)));
    if ((rawFlags & ~static_cast<qulonglong>(kSupportedCorrectionMask)) != 0ULL) {
        return fallback;
    }
    return static_cast<EphemerisCorrectionFlags>(static_cast<std::uint32_t>(rawFlags));
}

void saveEphemerisUserSettings(QSettings& settings, const SkySettingsStore::EphemerisUserSettingsSnapshot& snapshot)
{
    settings.setValue(settingsKey("ephemeris/engineKind"), ephemerisEngineKindToString(snapshot.engineKind));
    settings.setValue(
        settingsKey("ephemeris/correctionFlags"),
        static_cast<qulonglong>(static_cast<std::uint32_t>(snapshot.correctionFlags))
    );
    settings.setValue(settingsKey("ephemeris/correctionPresetId"), snapshot.correctionPresetId);
    settings.setValue(settingsKey("ephemeris/fallbackToSimpleEngine"), snapshot.fallbackToSimpleEngine);
    settings.setValue(settingsKey("ephemeris/refractionEnabled"), snapshot.refractionEnabled);
    settings.setValue(settingsKey("ephemeris/atmosphericPressureHpa"), snapshot.atmosphericPressureHpa);
    settings.setValue(settingsKey("ephemeris/atmosphericTemperatureC"), snapshot.atmosphericTemperatureC);
    settings.setValue(settingsKey("ephemeris/relativeHumidity"), snapshot.relativeHumidity);
    settings.setValue(settingsKey("ephemeris/observingWavelengthMicrometers"), snapshot.observingWavelengthMicrometers);
    settings.setValue(settingsKey("ephemeris/preferredDataProfileId"), snapshot.preferredDataProfileId);
    settings.setValue(settingsKey("ephemeris/onlineUpdatesEnabled"), snapshot.onlineUpdatesEnabled);
    settings.setValue(settingsKey("ephemeris/updatePresetId"), snapshot.updatePresetId);
    settings.setValue(settingsKey("ephemeris/updateManifestUrl"), snapshot.updateManifestUrl);
}

SkySettingsStore::EphemerisUserSettingsSnapshot loadEphemerisUserSettings(QSettings& settings)
{
    SkySettingsStore::EphemerisUserSettingsSnapshot snapshot;
    snapshot.engineKind = ephemerisEngineKindFromString(
        settings.value(settingsKey("ephemeris/engineKind"), ephemerisEngineKindToString(snapshot.engineKind))
            .toString(),
        snapshot.engineKind
    );
    snapshot.correctionFlags =
        readCorrectionFlags(settings, settingsKey("ephemeris/correctionFlags"), snapshot.correctionFlags);
    snapshot.correctionPresetId =
        normalizedNonBlankSetting(settings, settingsKey("ephemeris/correctionPresetId"), snapshot.correctionPresetId);
    snapshot.fallbackToSimpleEngine =
        readBoolSetting(settings, settingsKey("ephemeris/fallbackToSimpleEngine"), snapshot.fallbackToSimpleEngine);
    snapshot.refractionEnabled =
        readBoolSetting(settings, settingsKey("ephemeris/refractionEnabled"), snapshot.refractionEnabled);
    snapshot.atmosphericPressureHpa =
        readDoubleSetting(settings, settingsKey("ephemeris/atmosphericPressureHpa"), snapshot.atmosphericPressureHpa);
    snapshot.atmosphericTemperatureC =
        readDoubleSetting(settings, settingsKey("ephemeris/atmosphericTemperatureC"), snapshot.atmosphericTemperatureC);
    snapshot.relativeHumidity =
        readDoubleSetting(settings, settingsKey("ephemeris/relativeHumidity"), snapshot.relativeHumidity);
    snapshot.observingWavelengthMicrometers = readDoubleSetting(
        settings, settingsKey("ephemeris/observingWavelengthMicrometers"), snapshot.observingWavelengthMicrometers
    );
    snapshot.preferredDataProfileId = normalizedNonBlankSetting(
        settings, settingsKey("ephemeris/preferredDataProfileId"), snapshot.preferredDataProfileId
    );
    if (snapshot.preferredDataProfileId == QStringLiteral("modern")) {
        snapshot.preferredDataProfileId = QStringLiteral("de440s-short-range");
    }
    snapshot.onlineUpdatesEnabled =
        readBoolSetting(settings, settingsKey("ephemeris/onlineUpdatesEnabled"), snapshot.onlineUpdatesEnabled);
    snapshot.updatePresetId =
        normalizedNonBlankSetting(settings, settingsKey("ephemeris/updatePresetId"), snapshot.updatePresetId);
    snapshot.updateManifestUrl =
        settings.value(settingsKey("ephemeris/updateManifestUrl"), snapshot.updateManifestUrl).toString().trimmed();
    return snapshot;
}

[[nodiscard]] bool hasEphemerisUserSettings(QSettings& settings)
{
    return settings.contains(settingsKey("ephemeris/engineKind"))
           || settings.contains(settingsKey("ephemeris/correctionFlags"))
           || settings.contains(settingsKey("ephemeris/correctionPresetId"))
           || settings.contains(settingsKey("ephemeris/fallbackToSimpleEngine"))
           || settings.contains(settingsKey("ephemeris/refractionEnabled"))
           || settings.contains(settingsKey("ephemeris/atmosphericPressureHpa"))
           || settings.contains(settingsKey("ephemeris/atmosphericTemperatureC"))
           || settings.contains(settingsKey("ephemeris/relativeHumidity"))
           || settings.contains(settingsKey("ephemeris/observingWavelengthMicrometers"))
           || settings.contains(settingsKey("ephemeris/preferredDataProfileId"))
           || settings.contains(settingsKey("ephemeris/onlineUpdatesEnabled"))
           || settings.contains(settingsKey("ephemeris/updatePresetId"))
           || settings.contains(settingsKey("ephemeris/updateManifestUrl"));
}

}  // namespace

SkyStateSettingsSnapshot splitStateSnapshot(const SkySettingsStore::StateSnapshot& snapshot)
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
    saveEphemerisUserSettings(settings, domains.ephemeris);
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
    domains.ephemerisSettingsPresent = hasEphemerisUserSettings(settings);
    if (domains.ephemerisSettingsPresent) {
        domains.ephemeris = loadEphemerisUserSettings(settings);
    }
    return mergeStateSnapshot(domains);
}

}  // namespace skygate::ui::internal
