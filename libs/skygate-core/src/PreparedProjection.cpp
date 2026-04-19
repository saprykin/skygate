#include "skygate/core/PreparedProjection.hpp"

#include "projections/ProjectionPipeline.hpp"
#include "skygate/core/math/MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

std::optional<PreparedProjection> PreparedProjection::create(
    const ProjectionType projectionType,
    const ProjectionParams& params
) noexcept
{
    if (!params.isProjectable()) {
        return std::nullopt;
    }

    PreparedProjection preparedProjection;
    preparedProjection.m_projectionType = projectionType;
    preparedProjection.m_params = params;
    preparedProjection.m_params.center = params.center.normalizedAzimuth();
    if (!ProjectionMath::tryBuildProjectionBasis(
            preparedProjection.m_params.center,
            preparedProjection.m_center,
            preparedProjection.m_right,
            preparedProjection.m_up
        )) {
        return std::nullopt;
    }

    switch (projectionType) {
    case ProjectionType::Stereographic: {
        const double halfFovRad = ProjectionMath::toRadians(preparedProjection.m_params.fovDeg) * 0.5;
        const double maxRadius = 2.0 * std::tan(halfFovRad * 0.5);
        if (!ProjectionMath::isFinite(maxRadius) || maxRadius <= MathConstants::kEpsilon) {
            return std::nullopt;
        }
        preparedProjection.m_circularMaxRadius = maxRadius;
        break;
    }
    case ProjectionType::AzimuthalEquidistant: {
        const double maxRadius = ProjectionMath::toRadians(preparedProjection.m_params.fovDeg) * 0.5;
        if (!ProjectionMath::isFinite(maxRadius) || maxRadius <= MathConstants::kEpsilon) {
            return std::nullopt;
        }
        preparedProjection.m_circularMaxRadius = maxRadius;
        break;
    }
    case ProjectionType::Perspective: {
        const double halfVerticalFovRad =
            ProjectionMath::toRadians(preparedProjection.m_params.fovDeg) * 0.5;
        const double tanHalfVerticalFov = std::tan(halfVerticalFovRad);
        if (
            !ProjectionMath::isFinite(tanHalfVerticalFov)
            || tanHalfVerticalFov <= MathConstants::kEpsilon
        ) {
            return std::nullopt;
        }

        const double aspectRatio =
            preparedProjection.m_params.viewportWidth / preparedProjection.m_params.viewportHeight;
        const double tanHalfHorizontalFov = tanHalfVerticalFov * aspectRatio;
        if (
            !ProjectionMath::isFinite(tanHalfHorizontalFov)
            || tanHalfHorizontalFov <= MathConstants::kEpsilon
        ) {
            return std::nullopt;
        }

        preparedProjection.m_rectHalfHeight = tanHalfVerticalFov;
        preparedProjection.m_rectHalfWidth = tanHalfHorizontalFov;
        break;
    }
    }

    return preparedProjection;
}

ProjectionType PreparedProjection::type() const noexcept
{
    return m_projectionType;
}

const ProjectionParams& PreparedProjection::params() const noexcept
{
    return m_params;
}

ScreenPoint PreparedProjection::project(const HorizontalCoordinate& coordinate) const noexcept
{
    if (!coordinate.isValid()) {
        return ProjectionPipeline::invalidCoordinatePoint();
    }

    const ProjectionMath::Vec3 target =
        ProjectionMath::horizontalToUnitVector(coordinate.normalizedAzimuth());

    switch (m_projectionType) {
    case ProjectionType::Stereographic: {
        const double centerDotTarget = std::clamp(
            ProjectionMath::dot(target, m_center),
            -1.0,
            1.0
        );
        const double denominator = 1.0 + centerDotTarget;
        if (denominator <= 0.0) {
            return ProjectionPipeline::culledPoint();
        }

        double projectedX = 2.0 * ProjectionMath::dot(target, m_right) / denominator;
        double projectedY = 2.0 * ProjectionMath::dot(target, m_up) / denominator;
        ProjectionMath::applyRoll(projectedX, projectedY, m_params.rollDeg);
        return ProjectionPipeline::finishCircular(
            projectedX,
            projectedY,
            m_params,
            m_circularMaxRadius
        );
    }
    case ProjectionType::AzimuthalEquidistant: {
        const double cosAngularDistance = std::clamp(
            ProjectionMath::dot(target, m_center),
            -1.0,
            1.0
        );
        const double angularDistance = std::acos(cosAngularDistance);
        if (!ProjectionMath::isFinite(angularDistance)) {
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
            projectedX = scale * ProjectionMath::dot(target, m_right);
            projectedY = scale * ProjectionMath::dot(target, m_up);
        }

        ProjectionMath::applyRoll(projectedX, projectedY, m_params.rollDeg);
        return ProjectionPipeline::finishCircular(
            projectedX,
            projectedY,
            m_params,
            m_circularMaxRadius
        );
    }
    case ProjectionType::Perspective: {
        const double forward = ProjectionMath::dot(target, m_center);
        if (forward <= MathConstants::kEpsilon) {
            return ProjectionPipeline::culledPoint();
        }

        double projectedX = ProjectionMath::dot(target, m_right) / forward;
        double projectedY = ProjectionMath::dot(target, m_up) / forward;
        ProjectionMath::applyRoll(projectedX, projectedY, m_params.rollDeg);
        return ProjectionPipeline::finishRectangular(
            projectedX,
            projectedY,
            m_params,
            m_rectHalfWidth,
            m_rectHalfHeight
        );
    }
    }

    return ProjectionPipeline::invalidParametersPoint();
}

}  // namespace skygate::core
