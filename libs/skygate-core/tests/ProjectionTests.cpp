#include "TestSupport.hpp"
#include "skygate/core/ProjectionFactory.hpp"

#include <cmath>
#include <limits>
#include <memory>

namespace skygate::core::tests {

bool runProjectionTests()
{
    bool success = true;

    std::unique_ptr<skygate::core::IProjection> projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    success = expectTrue(projection != nullptr, "Stereographic projection should be created") && success;
    if (projection == nullptr) {
        return false;
    }

    std::unique_ptr<skygate::core::IProjection> unsupportedProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    success = expectTrue(unsupportedProjection == nullptr, "Unsupported projection type should return null") && success;

    std::unique_ptr<skygate::core::IProjection> perspectiveProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    success = expectTrue(perspectiveProjection != nullptr, "Perspective projection should be created") && success;
    if (perspectiveProjection == nullptr) {
        return false;
    }

    success = expectTrue(
        perspectiveProjection->type() == skygate::core::ProjectionType::Perspective,
        "Perspective projection should report perspective type"
    ) && success;

    success = expectTrue(
        projection->type() == skygate::core::ProjectionType::Stereographic,
        "Projection type should report stereographic"
    ) && success;

    skygate::core::ProjectionParams params;
    params.center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0};
    params.fovDeg = 90.0;
    params.rollDeg = 0.0;
    params.viewportWidth = 1100.0;
    params.viewportHeight = 760.0;

    const auto centeredPoint = projection->project(params.center, params);
    success = expectTrue(centeredPoint.isVisible, "Center direction should be visible") && success;
    success = expectNear(centeredPoint.x, params.viewportWidth * 0.5, 1e-6, "Center direction x should map to screen center") && success;
    success = expectNear(centeredPoint.y, params.viewportHeight * 0.5, 1e-6, "Center direction y should map to screen center") && success;

    const auto oppositePoint = projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = -45.0, .azimuthDeg = 0.0},
        params
    );
    success = expectTrue(!oppositePoint.isVisible, "Opposite direction should not be visible") && success;

    skygate::core::ProjectionParams invalidParams = params;
    invalidParams.fovDeg = 0.0;
    const auto invalidFovPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!invalidFovPoint.isVisible, "FOV=0 should return not visible") && success;

    invalidParams = params;
    invalidParams.fovDeg = 179.0;
    const auto invalidFovHighPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!invalidFovHighPoint.isVisible, "FOV>=179 should return not visible") && success;

    invalidParams = params;
    invalidParams.viewportWidth = 0.0;
    const auto invalidWidthPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!invalidWidthPoint.isVisible, "Zero viewport width should return not visible") && success;

    invalidParams = params;
    invalidParams.viewportHeight = -1.0;
    const auto invalidHeightPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!invalidHeightPoint.isVisible, "Negative viewport height should return not visible") && success;

    invalidParams = params;
    invalidParams.rollDeg = std::numeric_limits<double>::quiet_NaN();
    const auto nanRollPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!nanRollPoint.isVisible, "NaN roll should return not visible") && success;

    skygate::core::ProjectionParams narrowFovParams = params;
    narrowFovParams.center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0};
    narrowFovParams.fovDeg = 10.0;
    narrowFovParams.rollDeg = 0.0;
    narrowFovParams.viewportWidth = 1000.0;
    narrowFovParams.viewportHeight = 1000.0;
    const auto outsideFovPoint = projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = 50.0},
        narrowFovParams
    );
    success = expectTrue(!outsideFovPoint.isVisible, "Direction outside FOV should return not visible") && success;

    skygate::core::ProjectionParams rollParams = narrowFovParams;
    rollParams.fovDeg = 120.0;
    const auto rollBaselinePoint = projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 10.0, .azimuthDeg = 10.0},
        rollParams
    );
    success = expectTrue(rollBaselinePoint.isVisible, "Baseline roll point should be visible") && success;

    rollParams.rollDeg = 90.0;
    const auto rollRotatedPoint = projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 10.0, .azimuthDeg = 10.0},
        rollParams
    );
    success = expectTrue(rollRotatedPoint.isVisible, "Roll-rotated point should be visible") && success;

    if (rollBaselinePoint.isVisible && rollRotatedPoint.isVisible) {
        const double centerX = rollParams.viewportWidth * 0.5;
        const double centerY = rollParams.viewportHeight * 0.5;
        const double baselineOffsetX = rollBaselinePoint.x - centerX;
        const double baselineOffsetY = rollBaselinePoint.y - centerY;
        const double rotatedOffsetX = rollRotatedPoint.x - centerX;
        const double rotatedOffsetY = rollRotatedPoint.y - centerY;

        success = expectNear(
            rotatedOffsetX,
            baselineOffsetY,
            1e-5,
            "Roll 90 should rotate x offset to previous y offset"
        ) && success;
        success = expectNear(
            rotatedOffsetY,
            -baselineOffsetX,
            1e-5,
            "Roll 90 should rotate y offset to negative previous x offset"
        ) && success;
    }

    skygate::core::ProjectionParams zenithParams = params;
    zenithParams.center = {.altitudeDeg = 90.0, .azimuthDeg = 0.0};
    const auto zenithCenteredPoint = projection->project(zenithParams.center, zenithParams);
    success = expectTrue(zenithCenteredPoint.isVisible, "Zenith center should still produce a visible center point") && success;
    success = expectNear(
        zenithCenteredPoint.x,
        zenithParams.viewportWidth * 0.5,
        1e-6,
        "Zenith center x should map to screen center"
    ) && success;
    success = expectNear(
        zenithCenteredPoint.y,
        zenithParams.viewportHeight * 0.5,
        1e-6,
        "Zenith center y should map to screen center"
    ) && success;

    skygate::core::ProjectionParams perspectiveParams;
    perspectiveParams.center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0};
    perspectiveParams.fovDeg = 90.0;
    perspectiveParams.rollDeg = 0.0;
    perspectiveParams.viewportWidth = 1200.0;
    perspectiveParams.viewportHeight = 600.0;

    const auto perspectiveCenterPoint =
        perspectiveProjection->project(perspectiveParams.center, perspectiveParams);
    success = expectTrue(perspectiveCenterPoint.isVisible, "Perspective center direction should be visible") && success;
    success = expectNear(
        perspectiveCenterPoint.x,
        perspectiveParams.viewportWidth * 0.5,
        1e-6,
        "Perspective center x should map to screen center"
    ) && success;
    success = expectNear(
        perspectiveCenterPoint.y,
        perspectiveParams.viewportHeight * 0.5,
        1e-6,
        "Perspective center y should map to screen center"
    ) && success;

    const double halfVerticalFovRad = (perspectiveParams.fovDeg * 0.5) * (3.14159265358979323846 / 180.0);
    const double halfHorizontalFovDeg =
        std::atan(std::tan(halfVerticalFovRad) * (perspectiveParams.viewportWidth / perspectiveParams.viewportHeight))
        * (180.0 / 3.14159265358979323846);
    const double insideHalfHorizontalFovDeg = halfHorizontalFovDeg - 0.5;
    const double rightInsideAzimuthDeg = 360.0 - insideHalfHorizontalFovDeg;

    const auto perspectiveRightInsidePoint = perspectiveProjection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = rightInsideAzimuthDeg},
        perspectiveParams
    );
    success = expectTrue(perspectiveRightInsidePoint.isVisible, "Perspective right-inside point should be visible") && success;
    success = expectTrue(
        perspectiveRightInsidePoint.x > (perspectiveParams.viewportWidth * 0.98),
        "Perspective right-inside direction should map near viewport right edge"
    ) && success;

    const auto perspectiveTopEdgePoint = perspectiveProjection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = perspectiveParams.fovDeg * 0.5, .azimuthDeg = 0.0},
        perspectiveParams
    );
    success = expectTrue(perspectiveTopEdgePoint.isVisible, "Perspective top edge should be visible") && success;
    success = expectNear(
        perspectiveTopEdgePoint.y,
        0.0,
        1e-4,
        "Perspective top-edge direction should map to viewport top edge"
    ) && success;

    const auto perspectiveOutsidePoint = perspectiveProjection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = 360.0 - (halfHorizontalFovDeg + 0.5)},
        perspectiveParams
    );
    success = expectTrue(!perspectiveOutsidePoint.isVisible, "Perspective point outside horizontal FoV should be hidden") && success;

    return success;
}

}  // namespace skygate::core::tests
