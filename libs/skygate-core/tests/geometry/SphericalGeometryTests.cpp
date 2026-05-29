#include "math/SphericalGeometry.hpp"
#include "math/Vector3d.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance = 1e-6)
{
    return std::abs(value - expected) <= tolerance;
}

void compareVector(const skygate::core::Vector3d& actual, const skygate::core::Vector3d& expected)
{
    QVERIFY(isNear(actual.x, expected.x));
    QVERIFY(isNear(actual.y, expected.y));
    QVERIFY(isNear(actual.z, expected.z));
}

}  // namespace

class SphericalGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void horizontalCoordinatesMapToUnitVectors();
    void unitVectorsMapToHorizontalCoordinates();
    void buildsOrthonormalProjectionBasis();
    void projectionBasisNormalizesAzimuthAndSupportsNadir();
    void rejectsInvalidProjectionCenterWithoutMutatingOutputs();
};

void SphericalGeometryTests::horizontalCoordinatesMapToUnitVectors()
{
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 0.0}),
        {0.0, 1.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 90.0}),
        {1.0, 0.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 90.0, .azimuthDeg = 123.0}),
        {0.0, 0.0, 1.0}
    );
}

void SphericalGeometryTests::unitVectorsMapToHorizontalCoordinates()
{
    const skygate::core::HorizontalCoordinate north =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 0.0, .y = 1.0, .z = 0.0});
    QVERIFY(isNear(north.altitudeDeg, 0.0));
    QVERIFY(isNear(north.azimuthDeg, 0.0));

    const skygate::core::HorizontalCoordinate east =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 1.0, .y = 0.0, .z = 0.0});
    QVERIFY(isNear(east.altitudeDeg, 0.0));
    QVERIFY(isNear(east.azimuthDeg, 90.0));

    const skygate::core::HorizontalCoordinate west =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = -1.0, .y = 0.0, .z = 0.0});
    QVERIFY(isNear(west.altitudeDeg, 0.0));
    QVERIFY(isNear(west.azimuthDeg, 270.0));

    const skygate::core::HorizontalCoordinate zenith =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 0.0, .y = 0.0, .z = 1.0});
    QVERIFY(isNear(zenith.altitudeDeg, 90.0));
}

void SphericalGeometryTests::buildsOrthonormalProjectionBasis()
{
    skygate::core::Vector3d center;
    skygate::core::Vector3d right;
    skygate::core::Vector3d up;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 45.0, .azimuthDeg = 180.0}, center, right, up
        )
    );

    QVERIFY(isNear(center.length(), 1.0));
    QVERIFY(isNear(right.length(), 1.0));
    QVERIFY(isNear(up.length(), 1.0));
    QVERIFY(isNear(center.dot(right), 0.0));
    QVERIFY(isNear(center.dot(up), 0.0));
    QVERIFY(isNear(right.dot(up), 0.0));
}

void SphericalGeometryTests::projectionBasisNormalizesAzimuthAndSupportsNadir()
{
    skygate::core::Vector3d negativeAzimuthCenter;
    skygate::core::Vector3d negativeAzimuthRight;
    skygate::core::Vector3d negativeAzimuthUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 0.0, .azimuthDeg = -90.0}, negativeAzimuthCenter, negativeAzimuthRight, negativeAzimuthUp
        )
    );

    skygate::core::Vector3d wrappedAzimuthCenter;
    skygate::core::Vector3d wrappedAzimuthRight;
    skygate::core::Vector3d wrappedAzimuthUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 0.0, .azimuthDeg = 270.0}, wrappedAzimuthCenter, wrappedAzimuthRight, wrappedAzimuthUp
        )
    );

    compareVector(negativeAzimuthCenter, wrappedAzimuthCenter);
    compareVector(negativeAzimuthRight, wrappedAzimuthRight);
    compareVector(negativeAzimuthUp, wrappedAzimuthUp);

    skygate::core::Vector3d nadirCenter;
    skygate::core::Vector3d nadirRight;
    skygate::core::Vector3d nadirUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = -90.0, .azimuthDeg = 123.0}, nadirCenter, nadirRight, nadirUp
        )
    );

    QVERIFY(isNear(nadirCenter.length(), 1.0));
    QVERIFY(isNear(nadirRight.length(), 1.0));
    QVERIFY(isNear(nadirUp.length(), 1.0));
    QVERIFY(isNear(nadirCenter.dot(nadirRight), 0.0));
    QVERIFY(isNear(nadirCenter.dot(nadirUp), 0.0));
    QVERIFY(isNear(nadirRight.dot(nadirUp), 0.0));
}

void SphericalGeometryTests::rejectsInvalidProjectionCenterWithoutMutatingOutputs()
{
    skygate::core::Vector3d center{.x = 1.0, .y = 2.0, .z = 3.0};
    skygate::core::Vector3d right{.x = 4.0, .y = 5.0, .z = 6.0};
    skygate::core::Vector3d up{.x = 7.0, .y = 8.0, .z = 9.0};
    QVERIFY(!skygate::core::SphericalGeometry::tryBuildProjectionBasis(
        {.altitudeDeg = 120.0, .azimuthDeg = 0.0}, center, right, up
    ));

    compareVector(center, {1.0, 2.0, 3.0});
    compareVector(right, {4.0, 5.0, 6.0});
    compareVector(up, {7.0, 8.0, 9.0});
}

QTEST_APPLESS_MAIN(SphericalGeometryTests)

#include "SphericalGeometryTests.moc"
