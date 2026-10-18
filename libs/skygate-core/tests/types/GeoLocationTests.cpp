#include "GeoLocation.hpp"

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
    void equalsComparesEveryField();
    void isIdenticalToDistinguishesSignedZero();
};

void GeoLocationTests::originIsValid()
{
    const skygate::core::GeoLocation location{
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    QVERIFY(location.isValid());
}

void GeoLocationTests::boundaryCoordinatesAreValid()
{
    const skygate::core::GeoLocation location{
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

void GeoLocationTests::equalsComparesEveryField()
{
    const skygate::core::GeoLocation location{
        .latitudeDeg = 47.3769,
        .longitudeDeg = 8.5417,
        .elevationMeters = 408.0,
    };
    QVERIFY(location.equals(location));
    QVERIFY(location.equals({.latitudeDeg = 47.3769, .longitudeDeg = 8.5417, .elevationMeters = 408.0}));

    skygate::core::GeoLocation other = location;
    other.latitudeDeg += 1e-12;
    QVERIFY(!location.equals(other));

    other = location;
    other.longitudeDeg -= 1e-12;
    QVERIFY(!location.equals(other));

    other = location;
    other.elevationMeters += 1.0;
    QVERIFY(!location.equals(other));
}

void GeoLocationTests::isIdenticalToDistinguishesSignedZero()
{
    const skygate::core::GeoLocation positiveZeroLocation{
        .latitudeDeg = 47.3769,
        .longitudeDeg = 0.0,
        .elevationMeters = 408.0,
    };
    const skygate::core::GeoLocation negativeZeroLocation{
        .latitudeDeg = 47.3769,
        .longitudeDeg = -0.0,
        .elevationMeters = 408.0,
    };

    QVERIFY(positiveZeroLocation.equals(negativeZeroLocation));
    QVERIFY(!positiveZeroLocation.isIdenticalTo(negativeZeroLocation));
    QVERIFY(!negativeZeroLocation.isIdenticalTo(positiveZeroLocation));
    QVERIFY(
        positiveZeroLocation.isIdenticalTo({.latitudeDeg = 47.3769, .longitudeDeg = 0.0, .elevationMeters = 408.0})
    );
}

QTEST_APPLESS_MAIN(GeoLocationTests)

#include "GeoLocationTests.moc"
