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
    void outsideHorizontalFovIsHidden();
    void invalidParamsAreRejected();
};

void PerspectiveProjectionTests::centerDirectionMapsToScreenCenter()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();
    const auto projectedCenter = projection->project(params.center, params);

    QVERIFY(projectedCenter.isVisible);
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
    const double halfHorizontalFovDeg =
        qRadiansToDegrees(std::atan(std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)));
    const double insideHalfHorizontalFovDeg = halfHorizontalFovDeg - 0.5;
    const double rightInsideAzimuthDeg = 360.0 - insideHalfHorizontalFovDeg;

    const auto projectedRightInsidePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = rightInsideAzimuthDeg},
        params
    );

    QVERIFY(projectedRightInsidePoint.isVisible);
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
    QVERIFY(isNear(projectedTopEdgePoint.y, 0.0, 1e-4));
}

void PerspectiveProjectionTests::outsideHorizontalFovIsHidden()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg =
        qRadiansToDegrees(std::atan(std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)));
    const double outsideAzimuthDeg = 360.0 - (halfHorizontalFovDeg + 0.5);

    const auto projectedOutsidePoint = projection->project(
        {.altitudeDeg = 0.0, .azimuthDeg = outsideAzimuthDeg},
        params
    );

    QVERIFY(!projectedOutsidePoint.isVisible);
}

void PerspectiveProjectionTests::invalidParamsAreRejected()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(projection != nullptr);

    skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    params.fovDeg = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultPerspectiveParams();
    params.fovDeg = 179.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultPerspectiveParams();
    params.viewportWidth = 0.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultPerspectiveParams();
    params.viewportHeight = -1.0;
    QVERIFY(!projection->project(params.center, params).isVisible);

    params = makeDefaultPerspectiveParams();
    params.rollDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!projection->project(params.center, params).isVisible);
}

QTEST_APPLESS_MAIN(PerspectiveProjectionTests)

#include "PerspectiveProjectionTests.moc"
