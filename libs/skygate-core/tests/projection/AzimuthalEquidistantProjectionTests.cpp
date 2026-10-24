#include "projections/ProjectionAlgorithms.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

constexpr skygate::core::ProjectionType kProjectionType = skygate::core::ProjectionType::AzimuthalEquidistant;

[[nodiscard]] skygate::core::ScreenPoint
projectPoint(const skygate::core::HorizontalCoordinate& coordinate, const skygate::core::ProjectionParams& params)
{
    return skygate::core::ProjectionAlgorithms::project(kProjectionType, coordinate, params);
}

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

[[nodiscard]] skygate::core::ProjectionParams makeDefaultAzimuthalParams()
{
    return {
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1100.0,
        .viewportHeight = 760.0,
    };
}

}  // namespace

class AzimuthalEquidistantProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void radialDistanceIsLinearWithAngularDistance();
    void oppositeDirectionIsHidden();
    void directionAtCircularFovEdgeIsVisible();
    void directionOutsideFovIsHidden();
    void zenithCenterIsSupported();
};

void AzimuthalEquidistantProjectionTests::radialDistanceIsLinearWithAngularDistance()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 120.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto projectedPoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 330.0}, params);
    QVERIFY(projectedPoint.isVisible());

    const double centerX = params.viewportWidth * 0.5;
    const double centerY = params.viewportHeight * 0.5;
    const double projectedRadius = std::hypot(projectedPoint.x - centerX, projectedPoint.y - centerY);
    const double expectedRadius = 0.5 * params.viewportWidth * (30.0 / (params.fovDeg * 0.5));
    QVERIFY(isNear(projectedRadius, expectedRadius, 1e-4));
}

void AzimuthalEquidistantProjectionTests::oppositeDirectionIsHidden()
{
    const skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();
    const auto oppositePoint = projectPoint({.altitudeDeg = -45.0, .azimuthDeg = 0.0}, params);

    QVERIFY(!oppositePoint.isVisible());
    QCOMPARE(oppositePoint.status, skygate::core::ProjectionStatus::Culled);
}

void AzimuthalEquidistantProjectionTests::directionAtCircularFovEdgeIsVisible()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto edgePoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 30.0}, params);
    QVERIFY(edgePoint.isVisible());
    QCOMPARE(edgePoint.status, skygate::core::ProjectionStatus::Visible);

    const auto outsidePoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 30.25}, params);
    QVERIFY(!outsidePoint.isVisible());
    QCOMPARE(outsidePoint.status, skygate::core::ProjectionStatus::Culled);
}

void AzimuthalEquidistantProjectionTests::directionOutsideFovIsHidden()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 20.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto outsideFovPoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 50.0}, params);
    QVERIFY(!outsideFovPoint.isVisible());
    QCOMPARE(outsideFovPoint.status, skygate::core::ProjectionStatus::Culled);
}

void AzimuthalEquidistantProjectionTests::zenithCenterIsSupported()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 90.0, .azimuthDeg = 0.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1100.0,
        .viewportHeight = 760.0,
    };

    const auto projectedCenter = projectPoint(params.center, params);
    QVERIFY(projectedCenter.isVisible());
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

QTEST_APPLESS_MAIN(AzimuthalEquidistantProjectionTests)

#include "AzimuthalEquidistantProjectionTests.moc"
