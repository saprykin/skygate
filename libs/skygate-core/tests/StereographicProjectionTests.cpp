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
    void directionOutsideFovIsHidden();
    void rollRotatesProjectedPoint();
    void zenithCenterIsSupported();
};

void StereographicProjectionTests::centerDirectionMapsToScreenCenter()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultStereographicParams();
    const auto projectedCenter = projection->project(params.center, params);

    QVERIFY(projectedCenter.isVisible);
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
}

void StereographicProjectionTests::invalidParamsAreRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultStereographicParams();

    params.fovDeg = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultStereographicParams();
    params.fovDeg = 179.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultStereographicParams();
    params.viewportWidth = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultStereographicParams();
    params.viewportHeight = -1.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultStereographicParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!projection->project(params.center, params).isVisible);
}

void StereographicProjectionTests::directionOutsideFovIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 10.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto outsideFovPoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = 50.0},
        params
    );
    QVERIFY(!outsideFovPoint.isVisible);
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
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

QTEST_APPLESS_MAIN(StereographicProjectionTests)

#include "StereographicProjectionTests.moc"
