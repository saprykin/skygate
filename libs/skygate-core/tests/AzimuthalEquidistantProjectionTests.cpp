#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

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
    void directionOutsideFovIsHidden();
    void rollRotatesProjectedPoint();
};

void AzimuthalEquidistantProjectionTests::centerDirectionMapsToScreenCenter()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();
    const auto projectedCenter = projection->project(params.center, params);

    QVERIFY(projectedCenter.isVisible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

void AzimuthalEquidistantProjectionTests::radialDistanceIsLinearWithAngularDistance()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 120.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    const auto projectedPoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = 330.0},
        params
    );
    QVERIFY(projectedPoint.isVisible);

    const double centerX = params.viewportWidth * 0.5;
    const double centerY = params.viewportHeight * 0.5;
    const double projectedRadius = std::hypot(projectedPoint.x - centerX, projectedPoint.y - centerY);
    const double expectedRadius = 0.5 * params.viewportWidth * (30.0 / (params.fovDeg * 0.5));
    QVERIFY(isNear(projectedRadius, expectedRadius, 1e-4));
}

void AzimuthalEquidistantProjectionTests::oppositeDirectionIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();
    const auto oppositePoint = projection->project(
        {.altitudeDeg = -45.0, .azimuthDeg = 0.0},
        params
    );

    QVERIFY(!oppositePoint.isVisible);
}

void AzimuthalEquidistantProjectionTests::invalidParamsAreRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultAzimuthalParams();

    params.fovDeg = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultAzimuthalParams();
    params.fovDeg = 179.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultAzimuthalParams();
    params.viewportWidth = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultAzimuthalParams();
    params.viewportHeight = -1.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultAzimuthalParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!projection->project(params.center, params).isVisible);
}

void AzimuthalEquidistantProjectionTests::directionOutsideFovIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
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

void AzimuthalEquidistantProjectionTests::rollRotatesProjectedPoint()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
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

QTEST_APPLESS_MAIN(AzimuthalEquidistantProjectionTests)

#include "AzimuthalEquidistantProjectionTests.moc"
