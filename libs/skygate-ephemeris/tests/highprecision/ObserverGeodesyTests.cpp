#include "engine/highprecision/ObserverGeodesy.hpp"

#include <QtTest/QtTest>
#include "skygate/core/math/PhysicalConstants.hpp"

#include <cmath>

namespace {

using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

using core::PhysicalConstants;
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
    const std::optional<SolarSystemKernelVector> position = observerItrsPositionAu(
        core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(position.has_value());
    QCOMPARE(position->xAu, kWgs84EquatorialRadiusMeters / PhysicalConstants::kAstronomicalUnitMeters);
    QCOMPARE(position->yAu, 0.0);
    QCOMPARE(position->zAu, 0.0);
}

void ObserverGeodesyTests::convertsNorthPoleToWgs84PolarRadius()
{
    const std::optional<SolarSystemKernelVector> position = observerItrsPositionAu(
        core::GeoLocation{
            .latitudeDeg = 90.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(position.has_value());
    QVERIFY(std::abs(position->xAu) < 1.0e-20);
    QCOMPARE(position->yAu, 0.0);
    QVERIFY(std::abs(position->zAu - kWgs84PolarRadiusMeters / PhysicalConstants::kAstronomicalUnitMeters) < 1.0e-16);
}

void ObserverGeodesyTests::includesElevationAlongLocalUp()
{
    const std::optional<SolarSystemKernelVector> seaLevel = observerItrsPositionAu(
        core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );
    const std::optional<SolarSystemKernelVector> elevated = observerItrsPositionAu(
        core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 4'200.0,
        }
    );

    QVERIFY(seaLevel.has_value());
    QVERIFY(elevated.has_value());
    QCOMPARE(elevated->xAu - seaLevel->xAu, 4'200.0 / PhysicalConstants::kAstronomicalUnitMeters);
    QCOMPARE(elevated->yAu, seaLevel->yAu);
    QCOMPARE(elevated->zAu, seaLevel->zAu);
}

void ObserverGeodesyTests::rejectsInvalidObservers()
{
    const std::optional<SolarSystemKernelVector> position = observerItrsPositionAu(
        core::GeoLocation{
            .latitudeDeg = core::GeoLocation::kLatitudeMaxDeg + 1.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(!position.has_value());
}

}  // namespace

QTEST_APPLESS_MAIN(ObserverGeodesyTests)

#include "ObserverGeodesyTests.moc"
