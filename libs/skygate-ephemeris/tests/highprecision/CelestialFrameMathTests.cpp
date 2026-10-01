#include "engine/highprecision/CelestialFrameMath.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <limits>

using skygate::core::EquatorialCoordinate;
using skygate::core::Vector3d;
using skygate::ephemeris::highprecision::CelestialFrameMath;

class CelestialFrameMathTests final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsEquatorialCoordinates();
    void normalizesRightAscension();
    void rejectsZeroAndNonfiniteVectors();
};

void CelestialFrameMathTests::roundTripsEquatorialCoordinates()
{
    const std::array coordinates{
        EquatorialCoordinate{0.0, 0.0},
        EquatorialCoordinate{6.0, 90.0},
        EquatorialCoordinate{18.0, -90.0},
        EquatorialCoordinate{23.999, -45.0},
        EquatorialCoordinate{7.25, 32.5},
    };
    for (const auto& coordinate : coordinates) {
        const Vector3d vector = CelestialFrameMath::fromEquatorial(coordinate);
        QVERIFY(std::abs(vector.length() - 1.0) < 1.0e-14);
        const auto result = CelestialFrameMath::toEquatorial(vector);
        QVERIFY(result.has_value());
        QVERIFY(std::abs(result->rightAscensionHours - coordinate.rightAscensionHours) < 1.0e-12);
        QVERIFY(std::abs(result->declinationDeg - coordinate.declinationDeg) < 1.0e-12);
    }
}

void CelestialFrameMathTests::normalizesRightAscension()
{
    const auto result = CelestialFrameMath::toEquatorial(Vector3d{0.0, -2.0, 0.0});
    QVERIFY(result.has_value());
    QCOMPARE(result->rightAscensionHours, 18.0);
    QCOMPARE(result->declinationDeg, 0.0);
}

void CelestialFrameMathTests::rejectsZeroAndNonfiniteVectors()
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double tiny = std::numeric_limits<double>::denorm_min();
    const std::array vectors{
        Vector3d{},
        Vector3d{tiny, 0.0, 0.0},
        Vector3d{infinity, 0.0, 0.0},
        Vector3d{0.0, nan, 0.0},
        Vector3d{0.0, 0.0, -infinity},
    };
    for (const auto& vector : vectors) {
        QVERIFY(!CelestialFrameMath::toEquatorial(vector).has_value());
    }
}

QTEST_APPLESS_MAIN(CelestialFrameMathTests)
#include "CelestialFrameMathTests.moc"
