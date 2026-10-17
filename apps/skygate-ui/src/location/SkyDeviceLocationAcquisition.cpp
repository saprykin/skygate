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
#if SKYGATE_HAS_POSITIONING
    if (livePositionSource() == positionSource) {
        m_positionSourceConfigured = true;
        return;
    }

    releasePositionSource();
    m_positionSourceGuard = positionSource;
    ++m_sourceRevision;
#else
    static_cast<void>(positionSource);
#endif
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

    if (livePositionSource() == nullptr && !m_positionSourceConfigured) {
        QGeoPositionInfoSource* const createdSource = QGeoPositionInfoSource::createDefaultSource(this);
        if (createdSource != nullptr) {
            m_positionSourceGuard = createdSource;
            m_ownedPositionSource = createdSource;
            ++m_sourceRevision;
        }
    }
    if (livePositionSource() == nullptr) {
        emit statusTextChanged(QStringLiteral("Location: Device source unavailable"));
        return;
    }

    connectPositionSource();

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
        const quint64 sourceRevision = m_sourceRevision;
        emit statusTextChanged(QStringLiteral("Location: Waiting for permission"));
        if (sourceRevision != m_sourceRevision) {
            return;
        }

        m_permissionGate->request(this, [this, sourceRevision](const Qt::PermissionStatus requestedStatus) {
            if (sourceRevision != m_sourceRevision) {
                return;
            }

            if (livePositionSource() == nullptr) {
                emit statusTextChanged(QStringLiteral("Location: Device source unavailable"));
                return;
            }

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

void SkyDeviceLocationAcquisition::connectPositionSource()
{
#if SKYGATE_HAS_POSITIONING
    QGeoPositionInfoSource* const source = livePositionSource();
    if (source == nullptr || m_sourceSignalsConnected) {
        return;
    }

    connect(
        source, &QGeoPositionInfoSource::positionUpdated, this, [this, source](const QGeoPositionInfo& positionInfo) {
            if (livePositionSource() == source) {
                applyPositionInfo(positionInfo);
            }
        }
    );
    connect(source, &QGeoPositionInfoSource::errorOccurred, this, [this, source](QGeoPositionInfoSource::Error) {
        if (livePositionSource() == source) {
            emit statusTextChanged(QStringLiteral("Location: Update failed"));
        }
    });
    m_sourceSignalsConnected = true;
#endif
}

QGeoPositionInfoSource* SkyDeviceLocationAcquisition::livePositionSource() noexcept
{
#if SKYGATE_HAS_POSITIONING
    if (m_positionSourceGuard.isNull()) {
        m_ownedPositionSource = nullptr;
        m_sourceSignalsConnected = false;
        return nullptr;
    }

    return static_cast<QGeoPositionInfoSource*>(m_positionSourceGuard.data());
#else
    return nullptr;
#endif
}

void SkyDeviceLocationAcquisition::releasePositionSource() noexcept
{
#if SKYGATE_HAS_POSITIONING
    QGeoPositionInfoSource* const source = livePositionSource();
    if (source != nullptr) {
        disconnect(source, nullptr, this, nullptr);
    }

    QGeoPositionInfoSource* const ownedSource = m_ownedPositionSource;
    m_ownedPositionSource = nullptr;
    delete ownedSource;

    m_positionSourceGuard = nullptr;
#endif
    m_sourceSignalsConnected = false;
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
    const quint64 sourceRevision = m_sourceRevision;
    emit statusTextChanged(QStringLiteral("Location: Locating"));
    if (sourceRevision != m_sourceRevision) {
        return;
    }

    QGeoPositionInfoSource* const source = livePositionSource();
    if (source == nullptr) {
        emit statusTextChanged(QStringLiteral("Location: Device source unavailable"));
        return;
    }

    connectPositionSource();
    source->requestUpdate(kUpdateTimeoutMs);
#endif
}
