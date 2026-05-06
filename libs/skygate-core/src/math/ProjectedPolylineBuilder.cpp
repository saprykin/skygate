#include "skygate/core/math/ProjectedPolylineBuilder.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/SphericalGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {
namespace {

constexpr int kMaxAdaptiveSubsegments = 512;
constexpr double kMinAngularStepDeg = 0.05;
constexpr double kMaxAngularStepDeg = 5.0;

[[nodiscard]] double angularDistanceRad(
    const SphericalGeometry::Vector3d& start,
    const SphericalGeometry::Vector3d& end
) noexcept
{
    return std::acos(std::clamp(SphericalGeometry::dot(start, end), -1.0, 1.0));
}

[[nodiscard]] int adaptiveSubsegmentCount(
    const ProjectionParams& params,
    const double angularDistance
) noexcept
{
    if (!std::isfinite(angularDistance) || angularDistance <= MathConstants::kEpsilon) {
        return 1;
    }

    const double angularStepDeg = std::clamp(
        params.fovDeg / 8.0,
        kMinAngularStepDeg,
        kMaxAngularStepDeg
    );
    const double angularDistanceDeg = AngleMath::toDegrees(angularDistance);
    return std::clamp(
        static_cast<int>(std::ceil(angularDistanceDeg / angularStepDeg)),
        1,
        kMaxAdaptiveSubsegments
    );
}

[[nodiscard]] SphericalGeometry::Vector3d interpolateUnitVector(
    const SphericalGeometry::Vector3d& start,
    const SphericalGeometry::Vector3d& end,
    const double angularDistance,
    const double t
) noexcept
{
    const double sinAngularDistance = std::sin(angularDistance);
    if (
        !std::isfinite(sinAngularDistance)
        || std::abs(sinAngularDistance) <= MathConstants::kEpsilon
    ) {
        return SphericalGeometry::normalize({
            start[0] + ((end[0] - start[0]) * t),
            start[1] + ((end[1] - start[1]) * t),
            start[2] + ((end[2] - start[2]) * t)
        });
    }

    const double startWeight = std::sin((1.0 - t) * angularDistance) / sinAngularDistance;
    const double endWeight = std::sin(t * angularDistance) / sinAngularDistance;
    return SphericalGeometry::normalize({
        (start[0] * startWeight) + (end[0] * endWeight),
        (start[1] * startWeight) + (end[1] * endWeight),
        (start[2] * startWeight) + (end[2] * endWeight)
    });
}

[[nodiscard]] HorizontalCoordinate horizontalFromUnitVector(
    const SphericalGeometry::Vector3d& vector
) noexcept
{
    const SphericalGeometry::Vector3d unit = SphericalGeometry::normalize(vector);
    return {
        .altitudeDeg = AngleMath::toDegrees(std::asin(std::clamp(unit[2], -1.0, 1.0))),
        .azimuthDeg = AngleMath::normalizeDegrees(AngleMath::toDegrees(std::atan2(unit[0], unit[1])))
    };
}

void appendProjectedSegment(
    std::vector<LineSegment2d>& segments,
    const ScreenPoint& start,
    const ScreenPoint& end,
    const double maxSegmentLengthSquared
)
{
    const double segmentLengthSquared = squaredDistance2d(
        end.x,
        end.y,
        start.x,
        start.y
    );
    if (segmentLengthSquared > maxSegmentLengthSquared) {
        return;
    }

    segments.push_back(LineSegment2d {
        .x1 = start.x,
        .y1 = start.y,
        .x2 = end.x,
        .y2 = end.y
    });
}

void appendAdaptiveProjectedSegment(
    std::vector<LineSegment2d>& segments,
    const PreparedProjection& projection,
    const HorizontalCoordinate& startCoordinate,
    const HorizontalCoordinate& endCoordinate,
    const double maxSegmentLengthSquared
)
{
    const SphericalGeometry::Vector3d startVector =
        SphericalGeometry::horizontalToUnitVector(startCoordinate.normalizedAzimuth());
    const SphericalGeometry::Vector3d endVector =
        SphericalGeometry::horizontalToUnitVector(endCoordinate.normalizedAzimuth());
    const double angularDistance = angularDistanceRad(startVector, endVector);
    const int subsegmentCount = adaptiveSubsegmentCount(projection.params(), angularDistance);

    ScreenPoint previousPoint = projection.project(startCoordinate);
    bool hasPreviousPoint = previousPoint.isVisible && previousPoint.isFinite();
    for (int subsegmentIndex = 1; subsegmentIndex <= subsegmentCount; ++subsegmentIndex) {
        const double t = static_cast<double>(subsegmentIndex) / static_cast<double>(subsegmentCount);
        const HorizontalCoordinate coordinate = subsegmentIndex == subsegmentCount
            ? endCoordinate
            : horizontalFromUnitVector(interpolateUnitVector(
                startVector,
                endVector,
                angularDistance,
                t
            ));
        const ScreenPoint point = projection.project(coordinate);
        const bool hasPoint = point.isVisible && point.isFinite();
        if (hasPreviousPoint && hasPoint) {
            appendProjectedSegment(segments, previousPoint, point, maxSegmentLengthSquared);
        }

        previousPoint = point;
        hasPreviousPoint = hasPoint;
    }
}

}  // namespace

std::vector<LineSegment2d> ProjectedPolylineBuilder::build(
    const PreparedProjection& projection,
    const std::span<const HorizontalCoordinate> coordinates,
    const double maxSegmentLengthSquared
) const
{
    std::vector<LineSegment2d> segments;
    if (coordinates.size() < 2U) {
        return segments;
    }

    if (!std::isfinite(maxSegmentLengthSquared) || maxSegmentLengthSquared < 0.0) {
        return segments;
    }

    bool hasPreviousCoordinate = false;
    HorizontalCoordinate previousCoordinate;
    for (const HorizontalCoordinate& coordinate : coordinates) {
        if (!coordinate.isValid()) {
            hasPreviousCoordinate = false;
            continue;
        }

        if (hasPreviousCoordinate) {
            appendAdaptiveProjectedSegment(
                segments,
                projection,
                previousCoordinate,
                coordinate,
                maxSegmentLengthSquared
            );
        }

        previousCoordinate = coordinate;
        hasPreviousCoordinate = true;
    }

    return segments;
}

}  // namespace skygate::core
