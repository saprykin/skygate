#include "SkyContextController.hpp"

#include "SkyCatalogManager.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"

#include <QDateTime>
#include <QTimeZone>

using namespace skygate::ui::internal;

bool SkyContextController::saveSettings() const
{
    if (m_settingsStore == nullptr) {
        return false;
    }

    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = m_live;
    snapshot.timelineToolbarCollapsed = m_timelineToolbarCollapsed;
    snapshot.searchToolbarCollapsed = m_searchToolbarCollapsed;
    snapshot.speedMultiplier = m_speedMultiplier;
    snapshot.stepSeconds = m_stepSeconds;
    snapshot.magnitudeCutoff = m_magnitudeCutoff;
    snapshot.viewCenterAltitudeDeg = m_viewCenterAltitudeDeg;
    snapshot.viewCenterAzimuthDeg = m_viewCenterAzimuthDeg;
    snapshot.viewFieldOfViewDeg = m_viewFieldOfViewDeg;
    snapshot.utcEpochSeconds =
        SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch();
    snapshot.latitudeDeg = m_skyContext.observer.latitudeDeg;
    snapshot.longitudeDeg = m_skyContext.observer.longitudeDeg;
    snapshot.elevationMeters = m_skyContext.observer.elevationMeters;
    snapshot.locationSourceText = locationSourceText();
    snapshot.selectedCityId = m_selectedCityId;
    snapshot.projectionTypeText = projectionTypeText();
    snapshot.catalogPresetIndex = catalogPresetIndex();
    snapshot.catalogUrlText = catalogUrlText();
    return m_settingsStore->saveState(snapshot);
}

bool SkyContextController::loadSettings()
{
    const auto stateSnapshot = m_settingsStore != nullptr
        ? m_settingsStore->loadState()
        : std::optional<SkySettingsStore::StateSnapshot> {};
    if (stateSnapshot.has_value()) {
        setLive(stateSnapshot->live);
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
        setCurrentUtc(QDateTime::fromSecsSinceEpoch(
            stateSnapshot->utcEpochSeconds,
            QTimeZone::UTC
        ));

        skygate::core::GeoLocation observer = m_skyContext.observer;
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
        setCatalogPresetIndex(stateSnapshot->catalogPresetIndex);
        setCatalogUrlText(stateSnapshot->catalogUrlText);
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
