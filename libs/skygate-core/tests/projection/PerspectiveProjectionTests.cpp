#include "projections/ProjectionAlgorithms.hpp"

#include <QtMath>
#include <QtTest/QtTest>

#include <cmath>

namespace {

constexpr skygate::core::ProjectionType kProjectionType = skygate::core::ProjectionType::Perspective;

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
};

void PerspectiveProjectionTests::centerDirectionMapsToScreenCenter()
{
    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();
    const auto projectedCenter = projectPoint(params.center, params);

    QVERIFY(projectedCenter.isVisible());
    QCOMPARE(projectedCenter.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedCenter.x, params.viewportWidth * 0.5, 1e-6));
    QVERIFY(isNear(projectedCenter.y, params.viewportHeight * 0.5, 1e-6));
}

void PerspectiveProjectionTests::insideHorizontalFovNearRightEdgeIsVisible()
{
    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg =
        qRadiansToDegrees(std::atan(std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)));
    const double insideHalfHorizontalFovDeg = halfHorizontalFovDeg - 0.5;
    const double rightInsideAzimuthDeg = 360.0 - insideHalfHorizontalFovDeg;

    const auto projectedRightInsidePoint =
        projectPoint({.altitudeDeg = 0.0, .azimuthDeg = rightInsideAzimuthDeg}, params);

    QVERIFY(projectedRightInsidePoint.isVisible());
    QCOMPARE(projectedRightInsidePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(projectedRightInsidePoint.x > (params.viewportWidth * 0.98));
}

void PerspectiveProjectionTests::topEdgeMapsToViewportTop()
{
    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const auto projectedTopEdgePoint = projectPoint({.altitudeDeg = params.fovDeg * 0.5, .azimuthDeg = 0.0}, params);

    QVERIFY(projectedTopEdgePoint.isVisible());
    QCOMPARE(projectedTopEdgePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(projectedTopEdgePoint.y, 0.0, 1e-4));
}

void PerspectiveProjectionTests::exactHorizontalFovEdgeIsVisible()
{
    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg =
        qRadiansToDegrees(std::atan(std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)));

    const auto edgePoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 360.0 - halfHorizontalFovDeg}, params);

    QVERIFY(edgePoint.isVisible());
    QCOMPARE(edgePoint.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(isNear(edgePoint.x, params.viewportWidth, 1e-4));

    const auto outsidePoint =
        projectPoint({.altitudeDeg = 0.0, .azimuthDeg = 360.0 - halfHorizontalFovDeg - 0.25}, params);
    QVERIFY(!outsidePoint.isVisible());
    QCOMPARE(outsidePoint.status, skygate::core::ProjectionStatus::Culled);
}

void PerspectiveProjectionTests::outsideHorizontalFovIsHidden()
{
    const skygate::core::ProjectionParams params = makeDefaultPerspectiveParams();

    const double halfVerticalFovRad = qDegreesToRadians(params.fovDeg * 0.5);
    const double halfHorizontalFovDeg =
        qRadiansToDegrees(std::atan(std::tan(halfVerticalFovRad) * (params.viewportWidth / params.viewportHeight)));
    const double outsideAzimuthDeg = 360.0 - (halfHorizontalFovDeg + 0.5);

    const auto projectedOutsidePoint = projectPoint({.altitudeDeg = 0.0, .azimuthDeg = outsideAzimuthDeg}, params);

    QVERIFY(!projectedOutsidePoint.isVisible());
    QCOMPARE(projectedOutsidePoint.status, skygate::core::ProjectionStatus::Culled);
}

QTEST_APPLESS_MAIN(PerspectiveProjectionTests)

#include "PerspectiveProjectionTests.moc"
