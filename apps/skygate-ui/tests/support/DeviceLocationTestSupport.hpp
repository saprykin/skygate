#pragma once

#include "location/ILocationPermissionGate.hpp"

#include <QDateTime>
#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#endif
#include <QTimeZone>

#include <functional>
#include <utility>

namespace {

QDateTime fakePositionTimestampUtc()
{
    return QDateTime(QDate(2026, 5, 6), QTime(9, 30, 0), QTimeZone::UTC);
}

#if SKYGATE_HAS_POSITIONING
class FakePositionSource final : public QGeoPositionInfoSource {
public:
    explicit FakePositionSource(QObject* parent = nullptr) : QGeoPositionInfoSource(parent) {}

    [[nodiscard]] PositioningMethods supportedPositioningMethods() const override
    {
        return SatellitePositioningMethods;
    }

    [[nodiscard]] int minimumUpdateInterval() const override
    {
        return 0;
    }

    [[nodiscard]] Error error() const override
    {
        return m_error;
    }

    [[nodiscard]] QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const override
    {
        (void)fromSatellitePositioningMethodsOnly;
        return m_lastPosition;
    }

    void startUpdates() override {}
    void stopUpdates() override {}

    void requestUpdate(const int timeoutMs = 0) override
    {
        m_lastTimeoutMs = timeoutMs;
        ++m_requestCount;
    }

    [[nodiscard]] int requestCount() const noexcept
    {
        return m_requestCount;
    }

    [[nodiscard]] int lastTimeoutMs() const noexcept
    {
        return m_lastTimeoutMs;
    }

    void publishPosition(const double latitudeDeg, const double longitudeDeg, const double altitudeMeters)
    {
        QGeoCoordinate coordinate(latitudeDeg, longitudeDeg, altitudeMeters);
        QGeoPositionInfo positionInfo(coordinate, fakePositionTimestampUtc());
        m_lastPosition = positionInfo;
        emit positionUpdated(positionInfo);
    }

    void publishInvalidPosition()
    {
        emit positionUpdated(QGeoPositionInfo());
    }

    void publishError(const Error error)
    {
        m_error = error;
        emit errorOccurred(error);
    }

private:
    Error m_error = NoError;
    QGeoPositionInfo m_lastPosition;
    int m_requestCount = 0;
    int m_lastTimeoutMs = 0;
};
#endif

class FakeLocationPermissionGate final : public skygate::ui::internal::ILocationPermissionGate {
public:
    [[nodiscard]] Qt::PermissionStatus status() const override
    {
        return m_status;
    }

    void request(QObject* context, std::function<void(Qt::PermissionStatus)> completion) override
    {
        static_cast<void>(context);
        m_completion = std::move(completion);
    }

    void setStatus(const Qt::PermissionStatus status) noexcept
    {
        m_status = status;
    }

    [[nodiscard]] bool hasPendingRequest() const noexcept
    {
        return m_completion != nullptr;
    }

    void completeRequest(const Qt::PermissionStatus status)
    {
        m_status = status;
        if (m_completion == nullptr) {
            return;
        }

        std::function<void(Qt::PermissionStatus)> completion = std::move(m_completion);
        m_completion = {};
        completion(status);
    }

private:
    Qt::PermissionStatus m_status = Qt::PermissionStatus::Undetermined;
    std::function<void(Qt::PermissionStatus)> m_completion;
};

}  // namespace
