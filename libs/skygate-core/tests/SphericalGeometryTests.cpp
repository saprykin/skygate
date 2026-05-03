#include "skygate/core/math/SphericalGeometry.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance = 1e-6)
{
    return std::abs(value - expected) <= tolerance;
}

void compareVector(
    const skygate::core::SphericalGeometry::Vector3d& actual,
    const skygate::core::SphericalGeometry::Vector3d& expected
)
{
    QVERIFY(isNear(actual[0], expected[0]));
    QVERIFY(isNear(actual[1], expected[1]));
    QVERIFY(isNear(actual[2], expected[2]));
}

}  // namespace

class SphericalGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void horizontalCoordinatesMapToUnitVectors();
    void normalizesVectorsAndPreservesZeroVector();
    void buildsOrthonormalProjectionBasis();
    void rejectsInvalidProjectionCenterWithoutMutatingOutputs();
};

void SphericalGeometryTests::horizontalCoordinatesMapToUnitVectors()
{
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({
            .altitudeDeg = 0.0,
            .azimuthDeg = 0.0
        }),
        {0.0, 1.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({
            .altitudeDeg = 0.0,
            .azimuthDeg = 90.0
        }),
        {1.0, 0.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({
            .altitudeDeg = 90.0,
            .azimuthDeg = 123.0
        }),
        {0.0, 0.0, 1.0}
    );
}

void SphericalGeometryTests::normalizesVectorsAndPreservesZeroVector()
{
    compareVector(skygate::core::SphericalGeometry::normalize({3.0, 4.0, 0.0}), {0.6, 0.8, 0.0});
    compareVector(skygate::core::SphericalGeometry::normalize({0.0, 0.0, 0.0}), {0.0, 0.0, 0.0});
}

void SphericalGeometryTests::buildsOrthonormalProjectionBasis()
{
    skygate::core::SphericalGeometry::Vector3d center;
    skygate::core::SphericalGeometry::Vector3d right;
    skygate::core::SphericalGeometry::Vector3d up;
    QVERIFY(skygate::core::SphericalGeometry::tryBuildProjectionBasis(
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        center,
        right,
        up
    ));

    QVERIFY(isNear(skygate::core::SphericalGeometry::length(center), 1.0));
    QVERIFY(isNear(skygate::core::SphericalGeometry::length(right), 1.0));
    QVERIFY(isNear(skygate::core::SphericalGeometry::length(up), 1.0));
    QVERIFY(isNear(skygate::core::SphericalGeometry::dot(center, right), 0.0));
    QVERIFY(isNear(skygate::core::SphericalGeometry::dot(center, up), 0.0));
    QVERIFY(isNear(skygate::core::SphericalGeometry::dot(right, up), 0.0));
}

void SphericalGeometryTests::rejectsInvalidProjectionCenterWithoutMutatingOutputs()
{
    skygate::core::SphericalGeometry::Vector3d center {1.0, 2.0, 3.0};
    skygate::core::SphericalGeometry::Vector3d right {4.0, 5.0, 6.0};
    skygate::core::SphericalGeometry::Vector3d up {7.0, 8.0, 9.0};
    QVERIFY(!skygate::core::SphericalGeometry::tryBuildProjectionBasis(
        {.altitudeDeg = 120.0, .azimuthDeg = 0.0},
        center,
        right,
        up
    ));

    compareVector(center, {1.0, 2.0, 3.0});
    compareVector(right, {4.0, 5.0, 6.0});
    compareVector(up, {7.0, 8.0, 9.0});
}

QTEST_APPLESS_MAIN(SphericalGeometryTests)

#include "SphericalGeometryTests.moc"
