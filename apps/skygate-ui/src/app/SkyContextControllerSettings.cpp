#include "SkyContextController.hpp"

#include "SkyCatalogManager.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include <QDateTime>
#include <QTimeZone>

using namespace skygate::ui::internal;

namespace {

[[nodiscard]] bool ephemerisOptionsEqual(
    const skygate::ephemeris::EphemerisEngineOptions& lhs, const skygate::ephemeris::EphemerisEngineOptions& rhs
) noexcept
{
    return lhs.engineKind == rhs.engineKind && lhs.correctionFlags == rhs.correctionFlags
           && lhs.fallbackToSimpleEngine == rhs.fallbackToSimpleEngine
           && lhs.enableAtmosphericRefraction == rhs.enableAtmosphericRefraction
           && lhs.atmosphericPressureHpa == rhs.atmosphericPressureHpa
           && lhs.atmosphericTemperatureC == rhs.atmosphericTemperatureC && lhs.relativeHumidity == rhs.relativeHumidity
           && lhs.observingWavelengthMicrometers == rhs.observingWavelengthMicrometers;
}

}  // namespace

bool SkyContextController::saveSettings() const
{
    if (m_settingsStore == nullptr) {
        return false;
    }

    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = m_timeline.live();
    snapshot.timelineToolbarCollapsed = m_timeline.toolbarCollapsed();
    snapshot.searchToolbarCollapsed = m_search.toolbarCollapsed();
    snapshot.speedMultiplier = m_timeline.speedMultiplier();
    snapshot.stepSeconds = m_timeline.stepSeconds();
    snapshot.magnitudeCutoff = m_view.magnitudeCutoff();
    snapshot.viewCenterAltitudeDeg = m_view.centerAltitudeDeg();
    snapshot.viewCenterAzimuthDeg = m_view.centerAzimuthDeg();
    snapshot.viewFieldOfViewDeg = m_view.fieldOfViewDeg();
    snapshot.utcEpochSeconds = SkyContextTimeCodec::toQDateTimeUtc(m_location.utcTime()).toSecsSinceEpoch();
    snapshot.latitudeDeg = m_location.observer().latitudeDeg;
    snapshot.longitudeDeg = m_location.observer().longitudeDeg;
    snapshot.elevationMeters = m_location.observer().elevationMeters;
    snapshot.locationSourceText = locationSourceText();
    snapshot.selectedCityId = m_location.selectedCityId();
    snapshot.displayTimeZoneId = m_timeController->timeZoneId();
    snapshot.projectionTypeText = projectionTypeText();
    snapshot.themeId = themeId();
    snapshot.overlayLayers = overlayLayerVisibility();
    snapshot.catalogPresetIndex = catalogPresetIndex();
    snapshot.catalogUrlText = catalogUrlText();
    snapshot.deepSkyCatalogPresetIndex = deepSkyCatalogPresetIndex();
    snapshot.deepSkyCatalogUrlText = deepSkyCatalogUrlText();
    snapshot.logToTerminal = logToTerminal();
    snapshot.logToFile = logToFile();
    snapshot.logFilePath = logFilePath();
    snapshot.ephemeris = m_ephemerisUserSettings;
    snapshot.ephemeris.engineKind = m_ephemerisEngineKind;
    snapshot.ephemeris.correctionFlags = m_ephemerisEngineOptions.correctionFlags;
    snapshot.ephemeris.fallbackToSimpleEngine = m_ephemerisEngineOptions.fallbackToSimpleEngine;
    snapshot.ephemeris.refractionEnabled = m_ephemerisEngineOptions.enableAtmosphericRefraction;
    snapshot.ephemeris.atmosphericPressureHpa = m_ephemerisEngineOptions.atmosphericPressureHpa;
    snapshot.ephemeris.atmosphericTemperatureC = m_ephemerisEngineOptions.atmosphericTemperatureC;
    snapshot.ephemeris.relativeHumidity = m_ephemerisEngineOptions.relativeHumidity;
    snapshot.ephemeris.observingWavelengthMicrometers = m_ephemerisEngineOptions.observingWavelengthMicrometers;
    snapshot.ephemerisSettingsPresent = true;
    return m_settingsStore->saveState(snapshot);
}

bool SkyContextController::loadSettings()
{
    const auto stateSnapshot =
        m_settingsStore != nullptr ? m_settingsStore->loadState() : std::optional<SkySettingsStore::StateSnapshot>{};
    if (stateSnapshot.has_value()) {
        setTimelineToolbarCollapsed(stateSnapshot->timelineToolbarCollapsed);
        setSearchToolbarCollapsed(stateSnapshot->searchToolbarCollapsed);
        setSpeedMultiplier(stateSnapshot->speedMultiplier);
        setStepSeconds(stateSnapshot->stepSeconds);
        setMagnitudeCutoff(stateSnapshot->magnitudeCutoff);
        setViewCenter(stateSnapshot->viewCenterAltitudeDeg, stateSnapshot->viewCenterAzimuthDeg);
        setViewFieldOfViewDeg(stateSnapshot->viewFieldOfViewDeg);
        if (stateSnapshot->live) {
            goLiveNow();
        } else {
            setLive(false);
            setCurrentUtc(QDateTime::fromSecsSinceEpoch(stateSnapshot->utcEpochSeconds, QTimeZone::UTC));
        }

        skygate::core::GeoLocation observer = m_location.observer();
        observer.latitudeDeg = stateSnapshot->latitudeDeg;
        observer.longitudeDeg = stateSnapshot->longitudeDeg;
        observer.elevationMeters = stateSnapshot->elevationMeters;
        if (observer.isValid()) {
            applyObserverLocation(observer);
        }

        const auto parsedLocationSource = SkyContextLocationSourceCodec::fromString(stateSnapshot->locationSourceText);
        SkyContextLocationSource locationSource = parsedLocationSource.has_value()
                                                      ? parsedLocationSource.value()
                                                      : SkyContextLocationSourceCodec::defaultSource();
        if (!SkyContextLocationSourceCodec::isAvailable(locationSource)) {
            locationSource = SkyContextLocationSource::Custom;
        }

        if (locationSource == SkyContextLocationSource::City && !applySelectedCityId(stateSnapshot->selectedCityId)) {
            setLocationSource(SkyContextLocationSource::Custom);
        } else {
            setLocationSource(locationSource);
        }

        setProjectionTypeText(stateSnapshot->projectionTypeText);
        m_timeController->setInitialTimeZoneId(stateSnapshot->displayTimeZoneId);
        setThemeId(stateSnapshot->themeId);
        if (m_overlayLayerSettings != nullptr) {
            m_overlayLayerSettings->setVisibility(stateSnapshot->overlayLayers);
        }
        setCatalogPresetIndex(stateSnapshot->catalogPresetIndex);
        setCatalogUrlText(stateSnapshot->catalogUrlText);
        setDeepSkyCatalogPresetIndex(stateSnapshot->deepSkyCatalogPresetIndex);
        setDeepSkyCatalogUrlText(stateSnapshot->deepSkyCatalogUrlText);
        setLogToTerminal(stateSnapshot->logToTerminal);
        setLogToFile(stateSnapshot->logToFile);
        setLogFilePath(stateSnapshot->logFilePath);

        if (stateSnapshot->ephemerisSettingsPresent) {
            const skygate::ephemeris::EphemerisEngineKind previousKind = m_ephemerisEngineKind;
            const skygate::ephemeris::EphemerisEngineOptions previousOptions = m_ephemerisEngineOptions;
            m_ephemerisUserSettings = stateSnapshot->ephemeris;
            m_ephemerisEngineKind = stateSnapshot->ephemeris.engineKind;
            m_ephemerisEngineOptions.engineKind = stateSnapshot->ephemeris.engineKind;
            m_ephemerisEngineOptions.correctionFlags = stateSnapshot->ephemeris.correctionFlags;
            m_ephemerisEngineOptions.fallbackToSimpleEngine = stateSnapshot->ephemeris.fallbackToSimpleEngine;
            m_ephemerisEngineOptions.enableAtmosphericRefraction = stateSnapshot->ephemeris.refractionEnabled;
            m_ephemerisEngineOptions.atmosphericPressureHpa = stateSnapshot->ephemeris.atmosphericPressureHpa;
            m_ephemerisEngineOptions.atmosphericTemperatureC = stateSnapshot->ephemeris.atmosphericTemperatureC;
            m_ephemerisEngineOptions.relativeHumidity = stateSnapshot->ephemeris.relativeHumidity;
            m_ephemerisEngineOptions.observingWavelengthMicrometers =
                stateSnapshot->ephemeris.observingWavelengthMicrometers;
            if (previousKind != m_ephemerisEngineKind
                || !ephemerisOptionsEqual(previousOptions, m_ephemerisEngineOptions)) {
                ++m_ephemerisOptionsRevision;
            }
            rebuildEphemerisEngine();
            emit skyContextChanged();
        }
    }

    if (m_catalogManager != nullptr) {
        m_catalogManager->restoreCatalogCache();
    }
    if (m_ephemerisDataManager != nullptr) {
        static_cast<void>(m_ephemerisDataManager->restoreFromSettings());
    }
    return stateSnapshot.has_value();
}

bool SkyContextController::clearCatalogCache()
{
    return m_catalogManager != nullptr && m_catalogManager->clearCatalogCache();
}

bool SkyContextController::clearDeepSkyCatalogCache()
{
    return m_catalogManager != nullptr && m_catalogManager->clearDeepSkyCatalogCache();
}
