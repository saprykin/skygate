#include "engine/highprecision/ObserverGeodesy.hpp"

#include <QtTest/QtTest>
#include "math/PhysicalConstants.hpp"

#include <cmath>

namespace {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

constexpr double kWgs84EquatorialRadiusMeters = 6'378'137.0;
constexpr double kWgs84PolarRadiusMeters = 6'356'752.314245179;

class ObserverGeodesyTests final : public QObject {
    Q_OBJECT

private slots:
    void convertsEquatorPrimeMeridianToWgs84EquatorialRadius();
    void convertsNorthPoleToWgs84PolarRadius();
    void includesElevationAlongLocalUp();
    void rejectsInvalidObservers();
};

void ObserverGeodesyTests::convertsEquatorPrimeMeridianToWgs84EquatorialRadius()
{
    const std::optional<Vector3d> position = observerItrsPositionAu(
        GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(position.has_value());
    QCOMPARE(position->x, kWgs84EquatorialRadiusMeters / PhysicalConstants::kAstronomicalUnitMeters);
    QCOMPARE(position->y, 0.0);
    QCOMPARE(position->z, 0.0);
}

void ObserverGeodesyTests::convertsNorthPoleToWgs84PolarRadius()
{
    const std::optional<Vector3d> position = observerItrsPositionAu(
        GeoLocation{
            .latitudeDeg = 90.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(position.has_value());
    QVERIFY(std::abs(position->x) < 1.0e-20);
    QCOMPARE(position->y, 0.0);
    QVERIFY(std::abs(position->z - kWgs84PolarRadiusMeters / PhysicalConstants::kAstronomicalUnitMeters) < 1.0e-16);
}

void ObserverGeodesyTests::includesElevationAlongLocalUp()
{
    const std::optional<Vector3d> seaLevel = observerItrsPositionAu(
        GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );
    const std::optional<Vector3d> elevated = observerItrsPositionAu(
        GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 4'200.0,
        }
    );

    QVERIFY(seaLevel.has_value());
    QVERIFY(elevated.has_value());
    QCOMPARE(elevated->x - seaLevel->x, 4'200.0 / PhysicalConstants::kAstronomicalUnitMeters);
    QCOMPARE(elevated->y, seaLevel->y);
    QCOMPARE(elevated->z, seaLevel->z);
}

void ObserverGeodesyTests::rejectsInvalidObservers()
{
    const std::optional<Vector3d> position = observerItrsPositionAu(
        GeoLocation{
            .latitudeDeg = GeoLocation::kLatitudeMaxDeg + 1.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(!position.has_value());
}

}  // namespace

QTEST_APPLESS_MAIN(ObserverGeodesyTests)

#include "ObserverGeodesyTests.moc"
