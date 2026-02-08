#include "skygate/core/Types.hpp"

#include <QtTest/QtTest>

#include <limits>

class GeoLocationTests final : public QObject {
    Q_OBJECT

private slots:
    void originIsValid();
    void boundaryCoordinatesAreValid();
    void outOfRangeLatitudeIsInvalid();
    void outOfRangeLongitudeIsInvalid();
    void nonFiniteValuesAreInvalid();
};

void GeoLocationTests::originIsValid()
{
    const skygate::core::GeoLocation location {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    QVERIFY(location.isValid());
}

void GeoLocationTests::boundaryCoordinatesAreValid()
{
    const skygate::core::GeoLocation location {
        .latitudeDeg = -90.0,
        .longitudeDeg = 180.0,
        .elevationMeters = -430.0,
    };

    QVERIFY(location.isValid());
}

void GeoLocationTests::outOfRangeLatitudeIsInvalid()
{
    skygate::core::GeoLocation location;
    location.latitudeDeg = 90.01;
    QVERIFY(!location.isValid());

    location.latitudeDeg = -90.01;
    QVERIFY(!location.isValid());
}

void GeoLocationTests::outOfRangeLongitudeIsInvalid()
{
    skygate::core::GeoLocation location;
    location.longitudeDeg = 180.01;
    QVERIFY(!location.isValid());

    location.longitudeDeg = -180.01;
    QVERIFY(!location.isValid());
}

void GeoLocationTests::nonFiniteValuesAreInvalid()
{
    skygate::core::GeoLocation location;
    location.latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!location.isValid());

    location = {};
    location.longitudeDeg = std::numeric_limits<double>::infinity();
    QVERIFY(!location.isValid());

    location = {};
    location.elevationMeters = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!location.isValid());
}

QTEST_APPLESS_MAIN(GeoLocationTests)

#include "GeoLocationTests.moc"
