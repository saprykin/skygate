#include "skygate/core/ProjectionFactory.hpp"

#include <QtMath>
#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

[[nodiscard]] skygate::core::ProjectionParams makeDefaultPerspectiveParams()
{
    return {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1200.0,
        .viewportHeight = 600.0,
    };
}

}  // namespace

class PerspectiveProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void centerDirectionMapsToScreenCenter();
    void insideHorizontalFovNearRightEdgeIsVisible();
    void topEdgeMapsToViewportTop();
    void exactHorizontalFovEdgeIsVisible();
    void outsideHorizontalFovIsHidden();
    void invalidParamsAreRejected();
    void invalidCoordinateIsRejected();
    void legalFovBoundaryValuesAreAccepted();
    void rollRotatesProjectedPoint();
    void zenithOrientationRemainsContinuous();
};

void PerspectiveProjectionTests::centerDirectionMapsToScreenCenter()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();
    const auto projectedCenter = projection->project(params.center, params);

    QVERIFY(projectedCenter.isVisible);
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

void PerspectiveProjectionTests::insideHorizontalFovNearRightEdgeIsVisible()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg = qRadiansToDegrees(std::atan(
        std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)
    ));
    const double insideHalfHorizontalFovDeg = halfHorizontalFovDeg - 0.5;
    const double rightInsideAzimuthDeg = 360.0 - insideHalfHorizontalFovDeg;

    const auto projectedRightInsidePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = rightInsideAzimuthDeg},
        params
    );

    QVERIFY(projectedRightInsidePoint.isVisible);
    QCOMPARE(projectedRightInsidePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(projectedRightInsidePoint.x > (params.viewportWidth * 0.98));
}

void PerspectiveProjectionTests::topEdgeMapsToViewportTop()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const auto projectedTopEdgePoint = projection->project(
        {.altitudeDeg = params.fovDeg * 0.5, .azimuthDeg = 0.0},
        params
    );

    QVERIFY(projectedTopEdgePoint.isVisible);
    QCOMPARE(projectedTopEdgePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedTopEdgePoint.y, 0.0, 1e-4));
}

void PerspectiveProjectionTests::exactHorizontalFovEdgeIsVisible()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg = qRadiansToDegrees(std::atan(
        std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)
    ));

    const auto edgePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = 360.0 - halfHorizontalFovDeg},
        params
    );

    QVERIFY(edgePoint.isVisible);
    QCOMPARE(edgePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(edgePoint.x, params.viewportWidth, 1e-4));

    const auto outsidePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = 360.0 - halfHorizontalFovDeg - 0.25},
        params
    );
    QVERIFY(!outsidePoint.isVisible);
    QCOMPARE(outsidePoint.status, skygate::core::ProjectionStatus::Culled);
}

void PerspectiveProjectionTests::outsideHorizontalFovIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg = qRadiansToDegrees(std::atan(
        std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)
    ));
    const double outsideAzimuthDeg = 360.0 - (halfHorizontalFovDeg + 0.5);

    const auto projectedOutsidePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = outsideAzimuthDeg},
        params
    );

    QVERIFY(!projectedOutsidePoint.isVisible);
    QCOMPARE(projectedOutsidePoint.status, skygate::core::ProjectionStatus::Culled);
}

void PerspectiveProjectionTests::invalidParamsAreRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    params.fovDeg = 0.9;
    const auto tooSmallFov = projection->project(params.center, params);
    QVERIFY(!tooSmallFov.isVisible);
    QCOMPARE(tooSmallFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultPerspectiveParams();
    params.fovDeg = 150.1;
    const auto tooLargeFov = projection->project(params.center, params);
    QVERIFY(!tooLargeFov.isVisible);
    QCOMPARE(tooLargeFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultPerspectiveParams();
    params.viewportWidth = 0.0;
    const auto zeroWidth = projection->project(params.center, params);
    QVERIFY(!zeroWidth.isVisible);
    QCOMPARE(zeroWidth.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultPerspectiveParams();
    params.viewportHeight = -1.0;
    const auto negativeHeight = projection->project(params.center, params);
    QVERIFY(!negativeHeight.isVisible);
    QCOMPARE(negativeHeight.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultPerspectiveParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    const auto nanRoll = projection->project(params.center, params);
    QVERIFY(!nanRoll.isVisible);
    QCOMPARE(nanRoll.status, skygate::core::ProjectionStatus::InvalidParameters);
}

void PerspectiveProjectionTests::invalidCoordinateIsRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();
    const auto invalidCoordinatePoint = projection->project(
        {.altitudeDeg = -95.0, .azimuthDeg = 0.0},
        params
    );

    QVERIFY(!invalidCoordinatePoint.isVisible);
    QCOMPARE(invalidCoordinatePoint.status, skygate::core::ProjectionStatus::InvalidCoordinate);
}

void PerspectiveProjectionTests::legalFovBoundaryValuesAreAccepted()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg;
    const auto minFovPoint = projection->project(params.center, params);
    QVERIFY(minFovPoint.isVisible);
    QCOMPARE(minFovPoint.status, skygate::core::ProjectionStatus::Visible);

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMaxDeg;
    const auto maxFovPoint = projection->project(params.center, params);
    QVERIFY(maxFovPoint.isVisible);
    QCOMPARE(maxFovPoint.status, skygate::core::ProjectionStatus::Visible);
}

void PerspectiveProjectionTests::rollRotatesProjectedPoint()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::HorizontalCoordinate target {.altitudeDeg = 10.0, .azimuthDeg = 10.0};
    const skygate::core::ProjectionParams baseParams {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 120.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto baselinePoint = projection->project(target, baseParams);
    QVERIFY(baselinePoint.isVisible);

    skygate::core::ProjectionParams rollParams = baseParams;
    rollParams.rollDeg = 90.0;
    const auto rotatedPoint = projection->project(target, rollParams);
    QVERIFY(rotatedPoint.isVisible);

    const double centerX = rollParams.viewportWidth * 0.5;
    const double centerY = rollParams.viewportHeight * 0.5;
    const double baselineOffsetX = baselinePoint.x - centerX;
    const double baselineOffsetY = baselinePoint.y - centerY;
    const double rotatedOffsetX = rotatedPoint.x - centerX;
    const double rotatedOffsetY = rotatedPoint.y - centerY;

    QVERIFY(isNear(rotatedOffsetX, baselineOffsetY, 1e-5));
    QVERIFY(isNear(rotatedOffsetY, -baselineOffsetX, 1e-5));
}

void PerspectiveProjectionTests::zenithOrientationRemainsContinuous()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::HorizontalCoordinate target {.altitudeDeg = 89.0, .azimuthDeg = 0.0};
    const skygate::core::ProjectionParams nearPoleParams {
        .center = {.altitudeDeg = 89.999, .azimuthDeg = 90.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };
    const skygate::core::ProjectionParams poleParams {
        .center = {.altitudeDeg = 90.0, .azimuthDeg = 90.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto nearPolePoint = projection->project(target, nearPoleParams);
    const auto polePoint = projection->project(target, poleParams);

    QVERIFY(nearPolePoint.isVisible);
    QVERIFY(polePoint.isVisible);
    QVERIFY(isNear(nearPolePoint.x, polePoint.x, 1.0));
    QVERIFY(isNear(nearPolePoint.y, polePoint.y, 1.0));
}

QTEST_APPLESS_MAIN(PerspectiveProjectionTests)

#include "PerspectiveProjectionTests.moc"
