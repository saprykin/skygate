#include "location/SkyDeviceLocationAcquisition.hpp"
#include "support/DeviceLocationTestSupport.hpp"

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#endif
#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include <memory>

using namespace skygate::ui::internal;

class SkyDeviceLocationAcquisitionTests final : public QObject {
    Q_OBJECT

private slots:
    void reportsUnavailableSource();
#if SKYGATE_HAS_POSITIONING
    void requestsTimedUpdateWhenPermissionGranted();
    void ignoresInvalidPositionsAndReportsFailure();
    void reportsPermissionDenial();
    void waitsForUndeterminedPermission();
    void startsUpdateWhenGrantedAfterWaiting();
    void disablesPermissionHandshakeOnRequest();
    void replacesPositionSourceAndDisconnectsOld();
    void suppressesSignalsFromReplacedSource();
    void ignoresCompletionAfterSourceDestruction();
    void ignoresCompletionAfterNullSourceReplacement();
    void repeatedStartsKeepSingleSourceConnection();
#endif
};

void SkyDeviceLocationAcquisitionTests::reportsUnavailableSource()
{
    SkyDeviceLocationAcquisition acquisition;
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
#if SKYGATE_HAS_POSITIONING
    acquisition.setPositionSource(nullptr);
#endif

    acquisition.start();

#if SKYGATE_HAS_POSITIONING
    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Initializing"));
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Device source unavailable"));
#else
    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Positioning unavailable"));
#endif
}

#if SKYGATE_HAS_POSITIONING

void SkyDeviceLocationAcquisitionTests::requestsTimedUpdateWhenPermissionGranted()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Granted);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
    QGeoCoordinate acquiredCoordinate;
    int acquiredCount = 0;
    connect(
        &acquisition,
        &SkyDeviceLocationAcquisition::positionAcquired,
        &acquisition,
        [&acquiredCoordinate, &acquiredCount](const QGeoCoordinate& coordinate) {
            acquiredCoordinate = coordinate;
            ++acquiredCount;
        }
    );

    acquisition.start();

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Initializing"));
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Locating"));
    QCOMPARE(positionSource.requestCount(), 1);
    QCOMPARE(positionSource.lastTimeoutMs(), SkyDeviceLocationAcquisition::kUpdateTimeoutMs);

    positionSource.publishPosition(51.5074, -0.1278, 35.0);

    QCOMPARE(acquiredCount, 1);
    QCOMPARE(acquiredCoordinate.latitude(), 51.5074);
    QCOMPARE(acquiredCoordinate.longitude(), -0.1278);
    QCOMPARE(acquiredCoordinate.altitude(), 35.0);
}

void SkyDeviceLocationAcquisitionTests::ignoresInvalidPositionsAndReportsFailure()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Granted);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
    int acquiredCount = 0;
    connect(
        &acquisition,
        &SkyDeviceLocationAcquisition::positionAcquired,
        &acquisition,
        [&acquiredCount](const QGeoCoordinate&) { ++acquiredCount; }
    );

    acquisition.start();
    positionSource.publishInvalidPosition();

    QCOMPARE(acquiredCount, 0);
    QCOMPARE(statusSpy.count(), 2);

    positionSource.publishError(QGeoPositionInfoSource::UpdateTimeoutError);

    QCOMPARE(statusSpy.count(), 3);
    QCOMPARE(statusSpy.at(2).at(0).toString(), QString("Location: Update failed"));

    positionSource.publishPosition(47.3769, 8.5417, 408.0);

    QCOMPARE(acquiredCount, 1);
}

void SkyDeviceLocationAcquisitionTests::reportsPermissionDenial()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Denied);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);

    acquisition.start();

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Initializing"));
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Permission denied"));
    QCOMPARE(positionSource.requestCount(), 0);
}

void SkyDeviceLocationAcquisitionTests::waitsForUndeterminedPermission()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Undetermined);
    FakeLocationPermissionGate* permissionGateHandle = permissionGate.get();
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);

    acquisition.start();

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Waiting for permission"));
    QVERIFY(permissionGateHandle->hasPendingRequest());
    QCOMPARE(positionSource.requestCount(), 0);

    permissionGateHandle->completeRequest(Qt::PermissionStatus::Denied);

    QCOMPARE(statusSpy.count(), 3);
    QCOMPARE(statusSpy.at(2).at(0).toString(), QString("Location: Permission denied"));
    QCOMPARE(positionSource.requestCount(), 0);
}

void SkyDeviceLocationAcquisitionTests::startsUpdateWhenGrantedAfterWaiting()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Undetermined);
    FakeLocationPermissionGate* permissionGateHandle = permissionGate.get();
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);

    acquisition.start();
    permissionGateHandle->completeRequest(Qt::PermissionStatus::Granted);

    QCOMPARE(statusSpy.count(), 3);
    QCOMPARE(statusSpy.at(2).at(0).toString(), QString("Location: Locating"));
    QCOMPARE(positionSource.requestCount(), 1);
}

void SkyDeviceLocationAcquisitionTests::disablesPermissionHandshakeOnRequest()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Denied);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    acquisition.setPermissionRequestsEnabled(false);
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);

    acquisition.start();

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Locating"));
    QCOMPARE(positionSource.requestCount(), 1);
}

void SkyDeviceLocationAcquisitionTests::replacesPositionSourceAndDisconnectsOld()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Granted);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource firstSource;
    FakePositionSource secondSource;
    acquisition.setPositionSource(&firstSource);
    int acquiredCount = 0;
    connect(
        &acquisition,
        &SkyDeviceLocationAcquisition::positionAcquired,
        &acquisition,
        [&acquiredCount](const QGeoCoordinate&) { ++acquiredCount; }
    );

    acquisition.start();
    QCOMPARE(firstSource.requestCount(), 1);

    acquisition.setPositionSource(&secondSource);
    acquisition.start();

    QCOMPARE(firstSource.requestCount(), 1);
    QCOMPARE(secondSource.requestCount(), 1);

    secondSource.publishPosition(48.8566, 2.3522, 35.0);
    QCOMPARE(acquiredCount, 1);

    firstSource.publishPosition(51.5074, -0.1278, 35.0);
    QCOMPARE(acquiredCount, 1);
}

void SkyDeviceLocationAcquisitionTests::suppressesSignalsFromReplacedSource()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Granted);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource firstSource;
    FakePositionSource secondSource;
    acquisition.setPositionSource(&firstSource);
    acquisition.start();
    acquisition.setPositionSource(&secondSource);
    acquisition.start();
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
    int acquiredCount = 0;
    connect(
        &acquisition,
        &SkyDeviceLocationAcquisition::positionAcquired,
        &acquisition,
        [&acquiredCount](const QGeoCoordinate&) { ++acquiredCount; }
    );

    firstSource.publishPosition(51.5074, -0.1278, 35.0);
    firstSource.publishError(QGeoPositionInfoSource::UpdateTimeoutError);

    QCOMPARE(acquiredCount, 0);
    QCOMPARE(statusSpy.count(), 0);

    secondSource.publishError(QGeoPositionInfoSource::UpdateTimeoutError);

    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Update failed"));

    secondSource.publishPosition(47.3769, 8.5417, 408.0);

    QCOMPARE(acquiredCount, 1);
}

void SkyDeviceLocationAcquisitionTests::ignoresCompletionAfterSourceDestruction()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    FakeLocationPermissionGate* permissionGateHandle = permissionGate.get();
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    auto positionSource = std::make_unique<FakePositionSource>();
    acquisition.setPositionSource(positionSource.get());
    acquisition.start();
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
    QVERIFY(permissionGateHandle->hasPendingRequest());

    positionSource.reset();
    permissionGateHandle->completeRequest(Qt::PermissionStatus::Granted);

    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QString("Location: Device source unavailable"));
}

void SkyDeviceLocationAcquisitionTests::ignoresCompletionAfterNullSourceReplacement()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    FakeLocationPermissionGate* permissionGateHandle = permissionGate.get();
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    acquisition.start();
    QSignalSpy statusSpy(&acquisition, &SkyDeviceLocationAcquisition::statusTextChanged);
    QVERIFY(permissionGateHandle->hasPendingRequest());

    acquisition.setPositionSource(nullptr);
    permissionGateHandle->completeRequest(Qt::PermissionStatus::Granted);

    QCOMPARE(statusSpy.count(), 0);
    QCOMPARE(positionSource.requestCount(), 0);

    acquisition.start();

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(1).at(0).toString(), QString("Location: Device source unavailable"));
    QCOMPARE(positionSource.requestCount(), 0);
}

void SkyDeviceLocationAcquisitionTests::repeatedStartsKeepSingleSourceConnection()
{
    auto permissionGate = std::make_unique<FakeLocationPermissionGate>();
    permissionGate->setStatus(Qt::PermissionStatus::Granted);
    SkyDeviceLocationAcquisition acquisition(std::move(permissionGate));
    FakePositionSource positionSource;
    acquisition.setPositionSource(&positionSource);
    int acquiredCount = 0;
    connect(
        &acquisition,
        &SkyDeviceLocationAcquisition::positionAcquired,
        &acquisition,
        [&acquiredCount](const QGeoCoordinate&) { ++acquiredCount; }
    );

    acquisition.start();
    acquisition.start();

    QCOMPARE(positionSource.requestCount(), 2);

    positionSource.publishPosition(51.5074, -0.1278, 35.0);

    QCOMPARE(acquiredCount, 1);
}

#endif

QTEST_GUILESS_MAIN(SkyDeviceLocationAcquisitionTests)

#include "SkyDeviceLocationAcquisitionTests.moc"
