#include "skygate/core/math/ProjectionMath.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-9)
{
    return std::abs(actual - expected) <= tolerance;
}

void compareVector(
    const skygate::core::ProjectionMath::Vec3& actual,
    const skygate::core::ProjectionMath::Vec3& expected,
    const double tolerance = 1e-9
)
{
    QVERIFY(isNear(actual[0], expected[0], tolerance));
    QVERIFY(isNear(actual[1], expected[1], tolerance));
    QVERIFY(isNear(actual[2], expected[2], tolerance));
}

}  // namespace

class ProjectionMathTests final : public QObject {
    Q_OBJECT

private slots:
    void horizontalUnitVectorsFollowCardinalDirections();
    void normalizeHandlesRegularAndZeroVectors();
    void projectionBasisIsOrthonormal();
    void invalidCenterRejectsBasisBuild();
    void rollRotatesOffsetsCounterClockwise();
};

void ProjectionMathTests::horizontalUnitVectorsFollowCardinalDirections()
{
    compareVector(
        skygate::core::ProjectionMath::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 0.0}),
        {0.0, 1.0, 0.0}
    );
    compareVector(
        skygate::core::ProjectionMath::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 90.0}),
        {1.0, 0.0, 0.0}
    );
    compareVector(
        skygate::core::ProjectionMath::horizontalToUnitVector({.altitudeDeg = 90.0, .azimuthDeg = 123.0}),
        {0.0, 0.0, 1.0}
    );
    compareVector(
        skygate::core::ProjectionMath::horizontalToUnitVector({.altitudeDeg = -90.0, .azimuthDeg = 321.0}),
        {0.0, 0.0, -1.0}
    );
}

void ProjectionMathTests::normalizeHandlesRegularAndZeroVectors()
{
    compareVector(skygate::core::ProjectionMath::normalize({3.0, 4.0, 0.0}), {0.6, 0.8, 0.0});
    compareVector(skygate::core::ProjectionMath::normalize({0.0, 0.0, 0.0}), {0.0, 0.0, 0.0});
}

void ProjectionMathTests::projectionBasisIsOrthonormal()
{
    skygate::core::ProjectionMath::Vec3 center;
    skygate::core::ProjectionMath::Vec3 right;
    skygate::core::ProjectionMath::Vec3 up;
    QVERIFY(skygate::core::ProjectionMath::tryBuildProjectionBasis(
        {.altitudeDeg = 37.0, .azimuthDeg = 42.0},
        center,
        right,
        up
    ));

    QVERIFY(isNear(skygate::core::ProjectionMath::length(center), 1.0));
    QVERIFY(isNear(skygate::core::ProjectionMath::length(right), 1.0));
    QVERIFY(isNear(skygate::core::ProjectionMath::length(up), 1.0));
    QVERIFY(isNear(skygate::core::ProjectionMath::dot(center, right), 0.0));
    QVERIFY(isNear(skygate::core::ProjectionMath::dot(center, up), 0.0));
    QVERIFY(isNear(skygate::core::ProjectionMath::dot(right, up), 0.0));
}

void ProjectionMathTests::invalidCenterRejectsBasisBuild()
{
    skygate::core::ProjectionMath::Vec3 center {1.0, 2.0, 3.0};
    skygate::core::ProjectionMath::Vec3 right {4.0, 5.0, 6.0};
    skygate::core::ProjectionMath::Vec3 up {7.0, 8.0, 9.0};

    QVERIFY(!skygate::core::ProjectionMath::tryBuildProjectionBasis(
        {.altitudeDeg = 91.0, .azimuthDeg = 0.0},
        center,
        right,
        up
    ));
    compareVector(center, {1.0, 2.0, 3.0});
    compareVector(right, {4.0, 5.0, 6.0});
    compareVector(up, {7.0, 8.0, 9.0});
}

void ProjectionMathTests::rollRotatesOffsetsCounterClockwise()
{
    double x = 1.0;
    double y = 0.0;
    skygate::core::ProjectionMath::applyRoll(x, y, 90.0);
    QVERIFY(isNear(x, 0.0));
    QVERIFY(isNear(y, 1.0));

    skygate::core::ProjectionMath::applyRoll(x, y, -90.0);
    QVERIFY(isNear(x, 1.0));
    QVERIFY(isNear(y, 0.0));
}

QTEST_APPLESS_MAIN(ProjectionMathTests)

#include "ProjectionMathTests.moc"
