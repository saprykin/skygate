#include "ProjectionAlgorithms.hpp"

#include "ProjectionPipeline.hpp"
#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {
namespace {

[[nodiscard]] bool isFinite(const double value) noexcept
{
    return std::isfinite(value);
}

void applyRoll(double& x, double& y, const double rollDeg) noexcept
{
    const double rollRad = AngleMath::toRadians(rollDeg);
    const double cosRoll = std::cos(rollRad);
    const double sinRoll = std::sin(rollRad);
    const double rotatedX = (x * cosRoll) - (y * sinRoll);
    const double rotatedY = (x * sinRoll) + (y * cosRoll);
    x = rotatedX;
    y = rotatedY;
}

}  // namespace

bool ProjectionAlgorithms::prepareFrame(
    const ProjectionType projectionType,
    const ProjectionParams& params,
    ProjectionFrame& frame
) noexcept
{
    if (!params.isProjectable()) {
        return false;
    }

    frame = ProjectionFrame {};
    frame.projectionType = projectionType;
    frame.params = params;
    frame.params.center = params.center.normalizedAzimuth();
    if (!SphericalGeometry::tryBuildProjectionBasis(
            frame.params.center,
            frame.center,
            frame.right,
            frame.up
        )) {
        return false;
    }

    switch (projectionType) {
    case ProjectionType::Stereographic: {
        const double halfFovRad = AngleMath::toRadians(frame.params.fovDeg) * 0.5;
        const double maxRadius = 2.0 * std::tan(halfFovRad * 0.5);
        if (!isFinite(maxRadius) || maxRadius <= MathConstants::kEpsilon) {
            return false;
        }
        frame.circularMaxRadius = maxRadius;
        break;
    }
    case ProjectionType::AzimuthalEquidistant: {
        const double maxRadius = AngleMath::toRadians(frame.params.fovDeg) * 0.5;
        if (!isFinite(maxRadius) || maxRadius <= MathConstants::kEpsilon) {
            return false;
        }
        frame.circularMaxRadius = maxRadius;
        break;
    }
    case ProjectionType::Perspective: {
        const double halfVerticalFovRad = AngleMath::toRadians(frame.params.fovDeg) * 0.5;
        const double tanHalfVerticalFov = std::tan(halfVerticalFovRad);
        if (
            !isFinite(tanHalfVerticalFov)
            || tanHalfVerticalFov <= MathConstants::kEpsilon
        ) {
            return false;
        }

        const double aspectRatio = frame.params.viewportWidth / frame.params.viewportHeight;
        const double tanHalfHorizontalFov = tanHalfVerticalFov * aspectRatio;
        if (
            !isFinite(tanHalfHorizontalFov)
            || tanHalfHorizontalFov <= MathConstants::kEpsilon
        ) {
            return false;
        }

        frame.rectHalfHeight = tanHalfVerticalFov;
        frame.rectHalfWidth = tanHalfHorizontalFov;
        break;
    }
    }

    return true;
}

ScreenPoint ProjectionAlgorithms::project(
    const ProjectionFrame& frame,
    const HorizontalCoordinate& coordinate,
    const double marginPx
) noexcept
{
    if (!coordinate.isValid()) {
        return ProjectionPipeline::invalidCoordinatePoint();
    }

    const SphericalGeometry::Vector3d target =
        SphericalGeometry::horizontalToUnitVector(coordinate.normalizedAzimuth());

    switch (frame.projectionType) {
    case ProjectionType::Stereographic: {
        const double centerDotTarget = std::clamp(
            SphericalGeometry::dot(target, frame.center),
            -1.0,
            1.0
        );
        const double denominator = 1.0 + centerDotTarget;
        if (denominator <= 0.0) {
            return ProjectionPipeline::culledPoint();
        }

        double projectedX = 2.0 * SphericalGeometry::dot(target, frame.right) / denominator;
        double projectedY = 2.0 * SphericalGeometry::dot(target, frame.up) / denominator;
        applyRoll(projectedX, projectedY, frame.params.rollDeg);
        return ProjectionPipeline::finishCircular(
            projectedX,
            projectedY,
            frame.params,
            frame.circularMaxRadius,
            marginPx
        );
    }
    case ProjectionType::AzimuthalEquidistant: {
        const double cosAngularDistance = std::clamp(
            SphericalGeometry::dot(target, frame.center),
            -1.0,
            1.0
        );
        const double angularDistance = std::acos(cosAngularDistance);
        if (!isFinite(angularDistance)) {
            return ProjectionPipeline::invalidParametersPoint();
        }

        double projectedX = 0.0;
        double projectedY = 0.0;
        if (angularDistance > MathConstants::kEpsilon) {
            const double sinAngularDistance = std::sin(angularDistance);
            if (std::abs(sinAngularDistance) <= MathConstants::kEpsilon) {
                return ProjectionPipeline::culledPoint();
            }

            const double scale = angularDistance / sinAngularDistance;
            projectedX = scale * SphericalGeometry::dot(target, frame.right);
            projectedY = scale * SphericalGeometry::dot(target, frame.up);
        }

        applyRoll(projectedX, projectedY, frame.params.rollDeg);
        return ProjectionPipeline::finishCircular(
            projectedX,
            projectedY,
            frame.params,
            frame.circularMaxRadius,
            marginPx
        );
    }
    case ProjectionType::Perspective: {
        const double forward = SphericalGeometry::dot(target, frame.center);
        if (forward <= MathConstants::kEpsilon) {
            return ProjectionPipeline::culledPoint();
        }

        double projectedX = SphericalGeometry::dot(target, frame.right) / forward;
        double projectedY = SphericalGeometry::dot(target, frame.up) / forward;
        applyRoll(projectedX, projectedY, frame.params.rollDeg);
        return ProjectionPipeline::finishRectangular(
            projectedX,
            projectedY,
            frame.params,
            frame.rectHalfWidth,
            frame.rectHalfHeight,
            marginPx
        );
    }
    }

    return ProjectionPipeline::invalidParametersPoint();
}

ScreenPoint ProjectionAlgorithms::project(
    const ProjectionType projectionType,
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params,
    const double marginPx
) noexcept
{
    ProjectionFrame frame;
    if (!prepareFrame(projectionType, params, frame)) {
        return ProjectionPipeline::invalidParametersPoint();
    }

    return project(frame, coordinate, marginPx);
}

}  // namespace skygate::core
