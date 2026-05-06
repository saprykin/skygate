#include "SkyContextController.hpp"

#include "SkyCatalogManager.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include <QDateTime>
#include <QTimeZone>

using namespace skygate::ui::internal;

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
    snapshot.utcEpochSeconds =
        SkyContextTimeCodec::toQDateTimeUtc(m_location.utcTime()).toSecsSinceEpoch();
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
    return m_settingsStore->saveState(snapshot);
}

bool SkyContextController::loadSettings()
{
    const auto stateSnapshot = m_settingsStore != nullptr
        ? m_settingsStore->loadState()
        : std::optional<SkySettingsStore::StateSnapshot> {};
    if (stateSnapshot.has_value()) {
        setTimelineToolbarCollapsed(stateSnapshot->timelineToolbarCollapsed);
        setSearchToolbarCollapsed(stateSnapshot->searchToolbarCollapsed);
        setSpeedMultiplier(stateSnapshot->speedMultiplier);
        setStepSeconds(stateSnapshot->stepSeconds);
        setMagnitudeCutoff(stateSnapshot->magnitudeCutoff);
        setViewCenter(
            stateSnapshot->viewCenterAltitudeDeg,
            stateSnapshot->viewCenterAzimuthDeg
        );
        setViewFieldOfViewDeg(stateSnapshot->viewFieldOfViewDeg);
        if (stateSnapshot->live) {
            goLiveNow();
        } else {
            setLive(false);
            setCurrentUtc(QDateTime::fromSecsSinceEpoch(
                stateSnapshot->utcEpochSeconds,
                QTimeZone::UTC
            ));
        }

        skygate::core::GeoLocation observer = m_location.observer();
        observer.latitudeDeg = stateSnapshot->latitudeDeg;
        observer.longitudeDeg = stateSnapshot->longitudeDeg;
        observer.elevationMeters = stateSnapshot->elevationMeters;
        if (observer.isValid()) {
            applyObserverLocation(observer);
        }

        const auto parsedLocationSource = SkyContextLocationSourceCodec::fromString(
            stateSnapshot->locationSourceText
        );
        SkyContextLocationSource locationSource = parsedLocationSource.has_value()
            ? parsedLocationSource.value()
            : SkyContextLocationSourceCodec::defaultSource();
        if (!SkyContextLocationSourceCodec::isAvailable(locationSource)) {
            locationSource = SkyContextLocationSource::Custom;
        }

        if (
            locationSource == SkyContextLocationSource::City
            && !applySelectedCityId(stateSnapshot->selectedCityId)
        ) {
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
    }

    if (m_catalogManager != nullptr) {
        m_catalogManager->restoreCatalogCache();
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
