#include "SkyContextController.hpp"
#include "SkyContextLocationSource.hpp"
#include "location/SkyDeviceLocationAcquisition.hpp"

using namespace skygate::ui::internal;

void SkyContextController::setLatitudeText(const QString& latitudeText)
{
    bool isValidNumber = false;
    const double latitude = latitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.latitudeDeg = latitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit latitudeTextChanged();
        return;
    }

    if (m_location.observer().latitudeDeg == nextObserver.latitudeDeg) {
        return;
    }

    if (m_location.source() != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setLongitudeText(const QString& longitudeText)
{
    bool isValidNumber = false;
    const double longitude = longitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.longitudeDeg = longitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit longitudeTextChanged();
        return;
    }

    if (m_location.observer().longitudeDeg == nextObserver.longitudeDeg) {
        return;
    }

    if (m_location.source() != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setElevationText(const QString& elevationText)
{
    bool isValidNumber = false;
    const double elevation = elevationText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.elevationMeters = elevation;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit elevationTextChanged();
        return;
    }

    if (m_location.observer().elevationMeters == nextObserver.elevationMeters) {
        return;
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::applyObserverLocation(const skygate::core::GeoLocation& observer)
{
    if (!observer.isValid()) {
        return;
    }

    const bool latitudeChanged = m_location.observer().latitudeDeg != observer.latitudeDeg;
    const bool longitudeChanged = m_location.observer().longitudeDeg != observer.longitudeDeg;
    const bool elevationChanged = m_location.observer().elevationMeters != observer.elevationMeters;
    if (!latitudeChanged && !longitudeChanged && !elevationChanged) {
        return;
    }

    m_location.setObserver(observer);
    if (latitudeChanged) {
        emit latitudeTextChanged();
    }
    if (longitudeChanged) {
        emit longitudeTextChanged();
    }
    if (elevationChanged) {
        emit elevationTextChanged();
    }
    emit nightConditionsChanged();
    if (!recenterTrackedTarget(true)) {
        emit skyContextChanged();
    }
}

void SkyContextController::refreshCurrentLocation()
{
    if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    m_deviceLocationAcquisition->start();
}
