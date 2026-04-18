#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

[[nodiscard]] skygate::core::ProjectionParams makeDefaultStereographicParams()
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

class StereographicProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void centerDirectionMapsToScreenCenter();
    void oppositeDirectionIsHidden();
    void invalidParamsAreRejected();
    void invalidCoordinateIsRejected();
    void directionOutsideFovIsHidden();
    void rollRotatesProjectedPoint();
    void zenithCenterIsSupported();
    void zenithOrientationRemainsContinuous();
};

void StereographicProjectionTests::centerDirectionMapsToScreenCenter()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultStereographicParams();
    const auto projectedCenter = projection->project(params.center, params);

    QVERIFY(projectedCenter.isVisible);
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

void StereographicProjectionTests::oppositeDirectionIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultStereographicParams();
    const auto oppositePoint = projection->project(
        {.altitudeDeg = -45.0, .azimuthDeg = 0.0},
        params
    );

    QVERIFY(!oppositePoint.isVisible);
    QCOMPARE(oppositePoint.status, skygate::core::ProjectionStatus::Culled);
}

void StereographicProjectionTests::invalidParamsAreRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultStereographicParams();

    params.fovDeg = 19.9;
    const auto tooSmallFov = projection->project(params.center, params);
    QVERIFY(!tooSmallFov.isVisible);
    QCOMPARE(tooSmallFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultStereographicParams();
    params.fovDeg = 150.1;
    const auto tooLargeFov = projection->project(params.center, params);
    QVERIFY(!tooLargeFov.isVisible);
    QCOMPARE(tooLargeFov.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultStereographicParams();
    params.viewportWidth = 0.0;
    const auto zeroWidth = projection->project(params.center, params);
    QVERIFY(!zeroWidth.isVisible);
    QCOMPARE(zeroWidth.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultStereographicParams();
    params.viewportHeight = -1.0;
    const auto negativeHeight = projection->project(params.center, params);
    QVERIFY(!negativeHeight.isVisible);
    QCOMPARE(negativeHeight.status, skygate::core::ProjectionStatus::InvalidParameters);

    params = makeDefaultStereographicParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    const auto nanRoll = projection->project(params.center, params);
    QVERIFY(!nanRoll.isVisible);
    QCOMPARE(nanRoll.status, skygate::core::ProjectionStatus::InvalidParameters);
}

void StereographicProjectionTests::invalidCoordinateIsRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultStereographicParams();
    const auto invalidCoordinatePoint = projection->project(
        {.altitudeDeg = 95.0, .azimuthDeg = 0.0},
        params
    );

    QVERIFY(!invalidCoordinatePoint.isVisible);
    QCOMPARE(invalidCoordinatePoint.status, skygate::core::ProjectionStatus::InvalidCoordinate);
}

void StereographicProjectionTests::directionOutsideFovIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 20.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto outsideFovPoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = 50.0},
        params
    );
    QVERIFY(!outsideFovPoint.isVisible);
    QCOMPARE(outsideFovPoint.status, skygate::core::ProjectionStatus::Culled);
}

void StereographicProjectionTests::rollRotatesProjectedPoint()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
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

void StereographicProjectionTests::zenithCenterIsSupported()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 90.0, .azimuthDeg = 0.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1100.0,
        .viewportHeight = 760.0,
    };

    const auto projectedCenter = projection->project(params.center, params);
    QVERIFY(projectedCenter.isVisible);
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

void StereographicProjectionTests::zenithOrientationRemainsContinuous()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
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

QTEST_APPLESS_MAIN(StereographicProjectionTests)

#include "StereographicProjectionTests.moc"
