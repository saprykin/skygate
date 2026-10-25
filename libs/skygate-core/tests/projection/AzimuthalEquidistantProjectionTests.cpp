#include "projections/ProjectionAlgorithms.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

constexpr skygate::core::ProjectionType kProjectionType = skygate::core::ProjectionType::AzimuthalEquidistant;

[[nodiscard]] skygate::core::ScreenPoint
projectPoint(const skygate::core::HorizontalCoordinate& coordinate, const skygate::core::ProjectionParams& params)
{
    skygate::core::ProjectionFrame frame;
    if (!skygate::core::ProjectionAlgorithms::prepareFrame(kProjectionType, params, frame)) {
        return skygate::core::ScreenPoint::invalidParameters();
    }

    return skygate::core::ProjectionAlgorithms::project(frame, coordinate);
}

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

}  // namespace

class AzimuthalEquidistantProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void radialDistanceIsLinearWithAngularDistance();
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
