#pragma once

#include "ILocationPermissionGate.hpp"

#include <QObject>
#include <QString>

#include <memory>

class QGeoCoordinate;
class QGeoPositionInfo;
class QGeoPositionInfoSource;

namespace skygate::ui::internal {

// Runs one device location acquisition for the controller: resolves the
// platform source when none is configured, performs the permission handshake,
// and issues a single update with a timeout. The controller keeps the observer
// location, the location source selection, and the status text presentation.
class SkyDeviceLocationAcquisition final : public QObject {
    Q_OBJECT

public:
    static constexpr int kUpdateTimeoutMs = 5000;

    explicit SkyDeviceLocationAcquisition(QObject* parent = nullptr);
    SkyDeviceLocationAcquisition(std::unique_ptr<ILocationPermissionGate> permissionGate, QObject* parent = nullptr);

    // Uses `positionSource` for later acquisitions without taking ownership. A
    // null source disables the platform default, so acquisition reports the
    // device source as unavailable; when no source is configured, the platform
    // default is created on the first start.
    void setPositionSource(QGeoPositionInfoSource* positionSource) noexcept;
    // When disabled, the acquisition skips the permission handshake and starts
    // the update directly.
    void setPermissionRequestsEnabled(bool enabled) noexcept;
    void start();

signals:
    void statusTextChanged(const QString& statusText);
    void positionAcquired(const QGeoCoordinate& coordinate);

private:
    void applyPositionInfo(const QGeoPositionInfo& positionInfo);
    void requestOneShotUpdate();

private:
    std::unique_ptr<ILocationPermissionGate> m_ownedPermissionGate;
    ILocationPermissionGate* m_permissionGate = nullptr;
    QGeoPositionInfoSource* m_positionSource = nullptr;
    bool m_positionSourceConfigured = false;
    bool m_sourceSignalsConnected = false;
    bool m_permissionRequestsEnabled = true;
};

}  // namespace skygate::ui::internal
