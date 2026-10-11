#include "SkyDeviceLocationAcquisition.hpp"
#include "SkyLocationPermissionGate.hpp"

#include <QCoreApplication>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#endif

#include <utility>

using namespace skygate::ui::internal;

SkyDeviceLocationAcquisition::SkyDeviceLocationAcquisition(QObject* parent)
    : SkyDeviceLocationAcquisition(nullptr, parent)
{
}

SkyDeviceLocationAcquisition::SkyDeviceLocationAcquisition(
    std::unique_ptr<ILocationPermissionGate> permissionGate, QObject* parent
)
    : QObject(parent), m_ownedPermissionGate(std::move(permissionGate))
{
    if (m_ownedPermissionGate == nullptr) {
        m_ownedPermissionGate = std::make_unique<SkyLocationPermissionGate>();
    }

    m_permissionGate = m_ownedPermissionGate.get();
}

void SkyDeviceLocationAcquisition::setPositionSource(QGeoPositionInfoSource* positionSource) noexcept
{
    m_positionSource = positionSource;
    m_positionSourceConfigured = true;
}

void SkyDeviceLocationAcquisition::setPermissionRequestsEnabled(const bool enabled) noexcept
{
    m_permissionRequestsEnabled = enabled;
}

void SkyDeviceLocationAcquisition::start()
{
#if SKYGATE_HAS_POSITIONING
    emit statusTextChanged(QStringLiteral("Location: Initializing"));

    if (m_positionSource == nullptr && !m_positionSourceConfigured) {
        m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    }
    if (m_positionSource == nullptr) {
        emit statusTextChanged(QStringLiteral("Location: Device source unavailable"));
        return;
    }

    if (!m_sourceSignalsConnected) {
        connect(
            m_positionSource,
            &QGeoPositionInfoSource::positionUpdated,
            this,
            &SkyDeviceLocationAcquisition::applyPositionInfo
        );
        connect(m_positionSource, &QGeoPositionInfoSource::errorOccurred, this, [this](QGeoPositionInfoSource::Error) {
            emit statusTextChanged(QStringLiteral("Location: Update failed"));
        });
        m_sourceSignalsConnected = true;
    }

    if (!m_permissionRequestsEnabled) {
        requestOneShotUpdate();
        return;
    }

    if (QCoreApplication::instance() == nullptr) {
        emit statusTextChanged(QStringLiteral("Location: Positioning unavailable"));
        return;
    }

    const Qt::PermissionStatus permissionStatus = m_permissionGate->status();
    if (permissionStatus == Qt::PermissionStatus::Granted) {
        requestOneShotUpdate();
        return;
    }

    if (permissionStatus == Qt::PermissionStatus::Undetermined) {
        emit statusTextChanged(QStringLiteral("Location: Waiting for permission"));
        m_permissionGate->request(this, [this](const Qt::PermissionStatus requestedStatus) {
            if (requestedStatus == Qt::PermissionStatus::Granted) {
                requestOneShotUpdate();
                return;
            }

            emit statusTextChanged(QStringLiteral("Location: Permission denied"));
        });
        return;
    }

    emit statusTextChanged(QStringLiteral("Location: Permission denied"));
#else
    emit statusTextChanged(QStringLiteral("Location: Positioning unavailable"));
#endif
}

void SkyDeviceLocationAcquisition::applyPositionInfo(const QGeoPositionInfo& positionInfo)
{
#if SKYGATE_HAS_POSITIONING
    if (!positionInfo.isValid()) {
        return;
    }

    const QGeoCoordinate coordinate = positionInfo.coordinate();
    if (!coordinate.isValid()) {
        return;
    }

    emit positionAcquired(coordinate);
#else
    static_cast<void>(positionInfo);
#endif
}

void SkyDeviceLocationAcquisition::requestOneShotUpdate()
{
#if SKYGATE_HAS_POSITIONING
    emit statusTextChanged(QStringLiteral("Location: Locating"));
    m_positionSource->requestUpdate(kUpdateTimeoutMs);
#endif
}
