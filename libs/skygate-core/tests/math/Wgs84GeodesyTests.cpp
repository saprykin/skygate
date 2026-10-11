#include "GeoLocation.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/Wgs84Geodesy.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

constexpr double kEquatorialRadiusMeters = skygate::core::PhysicalConstants::kWgs84EquatorialRadiusMeters;
constexpr double kPolarRadiusMeters =
    kEquatorialRadiusMeters * (1.0 - skygate::core::PhysicalConstants::kWgs84Flattening);

}  // namespace

class Wgs84GeodesyTests final : public QObject {
    Q_OBJECT

private slots:
    void equatorOnPrimeMeridianUsesEquatorialRadius();
    void ninetiethLongitudeUsesEquatorialRadius();
    void polesUseEllipsoidPolarRadius();
    void elevationAtEquatorAddsToRadius();
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

QTEST_APPLESS_MAIN(Wgs84GeodesyTests)

#include "Wgs84GeodesyTests.moc"
