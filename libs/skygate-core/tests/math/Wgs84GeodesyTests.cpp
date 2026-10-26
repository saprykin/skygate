#include "GeoLocation.hpp"
#include "math/AngleMath.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/Wgs84Geodesy.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

constexpr double kEquatorialRadiusMeters = skygate::core::PhysicalConstants::kWgs84EquatorialRadiusMeters;
constexpr double kPolarRadiusMeters =
    kEquatorialRadiusMeters * (1.0 - skygate::core::PhysicalConstants::kWgs84Flattening);

[[nodiscard]] skygate::core::Vector3d
localAxesToEcef(const skygate::core::GeoLocation& location, const skygate::core::Vector3d& eastNorthUpMeters)
{
    const double latitudeRad = skygate::core::AngleMath::toRadians(location.latitudeDeg);
    const double longitudeRad = skygate::core::AngleMath::toRadians(location.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);

    const skygate::core::Vector3d east{.x = -sinLongitude, .y = cosLongitude, .z = 0.0};
    const skygate::core::Vector3d north{
        .x = -sinLatitude * cosLongitude, .y = -sinLatitude * sinLongitude, .z = cosLatitude
    };
    const skygate::core::Vector3d up{
        .x = cosLatitude * cosLongitude, .y = cosLatitude * sinLongitude, .z = sinLatitude
    };

    return (east * eastNorthUpMeters.x) + (north * eastNorthUpMeters.y) + (up * eastNorthUpMeters.z);
}

}  // namespace

class Wgs84GeodesyTests final : public QObject {
    Q_OBJECT

private slots:
    void equatorOnPrimeMeridianUsesEquatorialRadius();
    void ninetiethLongitudeUsesEquatorialRadius();
    void polesUseEllipsoidPolarRadius();
    void elevationAtEquatorAddsToRadius();
    void eastNorthUpPlacesRadialOffsetOnUpAxis();
    void ecefRoundTripsThroughEastNorthUp();
};

void Wgs84GeodesyTests::equatorOnPrimeMeridianUsesEquatorialRadius()
{
    const skygate::core::Vector3d position = skygate::core::Wgs84Geodesy::ecefMeters(
        skygate::core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QCOMPARE(position.x, kEquatorialRadiusMeters);
    QCOMPARE(position.y, 0.0);
    QCOMPARE(position.z, 0.0);
}

void Wgs84GeodesyTests::ninetiethLongitudeUsesEquatorialRadius()
{
    const skygate::core::Vector3d position = skygate::core::Wgs84Geodesy::ecefMeters(
        skygate::core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 90.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(std::abs(position.x) < 1e-9);
    QVERIFY(std::abs(position.y - kEquatorialRadiusMeters) < 1e-6);
    QCOMPARE(position.z, 0.0);
}

void Wgs84GeodesyTests::polesUseEllipsoidPolarRadius()
{
    const skygate::core::Vector3d northPole = skygate::core::Wgs84Geodesy::ecefMeters(
        skygate::core::GeoLocation{
            .latitudeDeg = 90.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );
    const skygate::core::Vector3d southPole = skygate::core::Wgs84Geodesy::ecefMeters(
        skygate::core::GeoLocation{
            .latitudeDeg = -90.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 0.0,
        }
    );

    QVERIFY(std::abs(northPole.x) < 1e-9);
    QVERIFY(std::abs(northPole.y) < 1e-9);
    QVERIFY(std::abs(northPole.z - kPolarRadiusMeters) < 1e-6);
    QVERIFY(std::abs(southPole.x) < 1e-9);
    QVERIFY(std::abs(southPole.y) < 1e-9);
    QVERIFY(std::abs(southPole.z + kPolarRadiusMeters) < 1e-6);
}

void Wgs84GeodesyTests::elevationAtEquatorAddsToRadius()
{
    const skygate::core::Vector3d position = skygate::core::Wgs84Geodesy::ecefMeters(
        skygate::core::GeoLocation{
            .latitudeDeg = 0.0,
            .longitudeDeg = 0.0,
            .elevationMeters = 4'200.0,
        }
    );

    QCOMPARE(position.x, kEquatorialRadiusMeters + 4'200.0);
    QCOMPARE(position.y, 0.0);
    QCOMPARE(position.z, 0.0);
}

void Wgs84GeodesyTests::eastNorthUpPlacesRadialOffsetOnUpAxis()
{
    const skygate::core::GeoLocation location{
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    const skygate::core::GeoLocation raisedLocation{
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 2'500.0,
    };

    const skygate::core::Vector3d groundPosition = skygate::core::Wgs84Geodesy::ecefMeters(location);
    const skygate::core::Vector3d raisedPosition = skygate::core::Wgs84Geodesy::ecefMeters(raisedLocation);
    const skygate::core::Vector3d local =
        skygate::core::Wgs84Geodesy::eastNorthUpMeters(location, raisedPosition - groundPosition);

    QVERIFY(std::abs(local.x) < 1e-9);
    QVERIFY(std::abs(local.y) < 1e-9);
    QVERIFY(std::abs(local.z - 2'500.0) < 1e-9);
}

void Wgs84GeodesyTests::ecefRoundTripsThroughEastNorthUp()
{
    const skygate::core::GeoLocation location{
        .latitudeDeg = 47.6062,
        .longitudeDeg = -122.3321,
        .elevationMeters = 54.0,
    };

    const skygate::core::Vector3d ecef = skygate::core::Wgs84Geodesy::ecefMeters(location);
    const skygate::core::Vector3d local = skygate::core::Wgs84Geodesy::eastNorthUpMeters(location, ecef);
    const skygate::core::Vector3d reconstructedEcef = localAxesToEcef(location, local);

    QVERIFY(std::abs(reconstructedEcef.x - ecef.x) < 1e-6);
    QVERIFY(std::abs(reconstructedEcef.y - ecef.y) < 1e-6);
    QVERIFY(std::abs(reconstructedEcef.z - ecef.z) < 1e-6);
}

QTEST_APPLESS_MAIN(Wgs84GeodesyTests)

#include "Wgs84GeodesyTests.moc"
