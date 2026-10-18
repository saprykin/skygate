#include "projections/ProjectionAlgorithms.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

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
    void centerDirectionMapsToScreenCenter();
    void radialDistanceIsLinearWithAngularDistance();
    void oppositeDirectionIsHidden();
    void invalidParamsAreRejected();
    void invalidCoordinateIsRejected();
    void legalFovBoundaryValuesAreAccepted();
    void directionAtCircularFovEdgeIsVisible();
    void directionOutsideFovIsHidden();
    void rollRotatesProjectedPoint();
    void zenithCenterIsSupported();
    void zenithOrientationRemainsContinuous();
};

void AzimuthalEquidistantProjectionTests::centerDirectionMapsToScreenCenter()
{
    const skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();
    const auto projectedCenter = projectPoint(params.center, params);

    QVERIFY(projectedCenter.isVisible());
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

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

void AzimuthalEquidistantProjectionTests::invalidParamsAreRejected()
{
    skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();

    params.fovDeg = 0.9;
    const auto tooSmallFov = projectPoint(params.center, params);
    QVERIFY(!tooSmallFov.isVisible());
    QCOMPARE(tooSmallFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultAzimuthalParams();
    params.fovDeg = 150.1;
    const auto tooLargeFov = projectPoint(params.center, params);
    QVERIFY(!tooLargeFov.isVisible());
    QCOMPARE(tooLargeFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultAzimuthalParams();
    params.viewportWidth = 0.0;
    const auto zeroWidth = projectPoint(params.center, params);
    QVERIFY(!zeroWidth.isVisible());
    QCOMPARE(zeroWidth.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultAzimuthalParams();
    params.viewportHeight = -1.0;
    const auto negativeHeight = projectPoint(params.center, params);
    QVERIFY(!negativeHeight.isVisible());
    QCOMPARE(negativeHeight.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultAzimuthalParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    const auto nanRoll = projectPoint(params.center, params);
    QVERIFY(!nanRoll.isVisible());
    QCOMPARE(nanRoll.status, skygate::core::ProjectionStatus::InvalidParameters);
}

void AzimuthalEquidistantProjectionTests::invalidCoordinateIsRejected()
{
    const skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();
    const auto invalidCoordinatePoint = projectPoint({.altitudeDeg = 95.0, .azimuthDeg = 0.0}, params);

    QVERIFY(!invalidCoordinatePoint.isVisible());
    QCOMPARE(invalidCoordinatePoint.status, skygate::core::ProjectionStatus::InvalidCoordinate);
}

void AzimuthalEquidistantProjectionTests::legalFovBoundaryValuesAreAccepted()
{
    skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg;
    const auto minFovPoint = projectPoint(params.center, params);
    QVERIFY(minFovPoint.isVisible());
    QCOMPARE(minFovPoint.status, skygate::core::ProjectionStatus::Visible);

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMaxDeg;
    const auto maxFovPoint = projectPoint(params.center, params);
    QVERIFY(maxFovPoint.isVisible());
    QCOMPARE(maxFovPoint.status, skygate::core::ProjectionStatus::Visible);
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

void AzimuthalEquidistantProjectionTests::rollRotatesProjectedPoint()
{
    const skygate::core::HorizontalCoordinate target{.altitudeDeg = 10.0, .azimuthDeg = 10.0};
    const skygate::core::ProjectionParams baseParams{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 120.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto baselinePoint = projectPoint(target, baseParams);
    QVERIFY(baselinePoint.isVisible());

    skygate::core::ProjectionParams rollParams = baseParams;
    rollParams.rollDeg = 90.0;
    const auto rotatedPoint = projectPoint(target, rollParams);
    QVERIFY(rotatedPoint.isVisible());

    const double centerX = rollParams.viewportWidth * 0.5;
    const double centerY = rollParams.viewportHeight * 0.5;
    const double baselineOffsetX = baselinePoint.x - centerX;
    const double baselineOffsetY = baselinePoint.y - centerY;
    const double rotatedOffsetX = rotatedPoint.x - centerX;
    const double rotatedOffsetY = rotatedPoint.y - centerY;

    QVERIFY(isNear(rotatedOffsetX, baselineOffsetY, 1e-5));
    QVERIFY(isNear(rotatedOffsetY, -baselineOffsetX, 1e-5));
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

void AzimuthalEquidistantProjectionTests::zenithOrientationRemainsContinuous()
{
    const skygate::core::HorizontalCoordinate target{.altitudeDeg = 89.0, .azimuthDeg = 0.0};
    const skygate::core::ProjectionParams nearPoleParams{
        .center = {.altitudeDeg = 89.999, .azimuthDeg = 90.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };
    const skygate::core::ProjectionParams poleParams{
        .center = {.altitudeDeg = 90.0, .azimuthDeg = 90.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto nearPolePoint = projectPoint(target, nearPoleParams);
    const auto polePoint = projectPoint(target, poleParams);

    QVERIFY(nearPolePoint.isVisible());
    QVERIFY(polePoint.isVisible());
    QVERIFY(isNear(nearPolePoint.x, polePoint.x, 1.0));
    QVERIFY(isNear(nearPolePoint.y, polePoint.y, 1.0));
}

QTEST_APPLESS_MAIN(AzimuthalEquidistantProjectionTests)

#include "AzimuthalEquidistantProjectionTests.moc"
