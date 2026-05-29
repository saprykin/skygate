#include "ProjectedPolylineBuilder.hpp"
#include "AngleMath.hpp"
#include "Geometry2d.hpp"
#include "MathConstants.hpp"
#include "SphericalGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {
namespace {

constexpr int kMaxAdaptiveSubsegments = 512;
constexpr double kMinAngularStepDeg = 0.05;
constexpr double kMaxAngularStepDeg = 5.0;

[[nodiscard]] double angularDistanceRad(const Vector3d& start, const Vector3d& end) noexcept
{
    return std::acos(std::clamp(start.dot(end), -1.0, 1.0));
}

[[nodiscard]] int adaptiveSubsegmentCount(const ProjectionParams& params, const double angularDistance) noexcept
{
    if (!std::isfinite(angularDistance) || angularDistance <= MathConstants::kEpsilon) {
        return 1;
    }

    const double angularStepDeg = std::clamp(params.fovDeg / 8.0, kMinAngularStepDeg, kMaxAngularStepDeg);
    const double angularDistanceDeg = AngleMath::toDegrees(angularDistance);
    return std::clamp(static_cast<int>(std::ceil(angularDistanceDeg / angularStepDeg)), 1, kMaxAdaptiveSubsegments);
}

[[nodiscard]] Vector3d
interpolateUnitVector(const Vector3d& start, const Vector3d& end, const double angularDistance, const double t) noexcept
{
    const double sinAngularDistance = std::sin(angularDistance);
    if (!std::isfinite(sinAngularDistance) || std::abs(sinAngularDistance) <= MathConstants::kEpsilon) {
        const Vector3d interpolated = start + ((end - start) * t);
        return interpolated.normalized().value_or(Vector3d{});
    }

    const double startWeight = std::sin((1.0 - t) * angularDistance) / sinAngularDistance;
    const double endWeight = std::sin(t * angularDistance) / sinAngularDistance;
    const Vector3d interpolated = (start * startWeight) + (end * endWeight);
    return interpolated.normalized().value_or(Vector3d{});
}

void appendProjectedSegment(
    std::vector<LineSegment2d>& segments,
    const ScreenPoint& start,
    const ScreenPoint& end,
    const double maxSegmentLengthSquared
)
{
    const double segmentLengthSquared = Geometry2d::squaredDistance2d(end.x, end.y, start.x, start.y);
    if (segmentLengthSquared > maxSegmentLengthSquared) {
        return;
    }

    segments.push_back(LineSegment2d{.x1 = start.x, .y1 = start.y, .x2 = end.x, .y2 = end.y});
}

void appendAdaptiveProjectedSegment(
    std::vector<LineSegment2d>& segments,
    const PreparedProjection& projection,
    const HorizontalCoordinate& startCoordinate,
    const HorizontalCoordinate& endCoordinate,
    const double maxSegmentLengthSquared
)
{
    const Vector3d startVector = SphericalGeometry::horizontalToUnitVector(startCoordinate.normalizedAzimuth());
    const Vector3d endVector = SphericalGeometry::horizontalToUnitVector(endCoordinate.normalizedAzimuth());
    const double angularDistance = angularDistanceRad(startVector, endVector);
    const int subsegmentCount = adaptiveSubsegmentCount(projection.params(), angularDistance);

    ScreenPoint previousPoint = projection.project(startCoordinate);
    bool hasPreviousPoint = previousPoint.isVisible && previousPoint.isFinite();
    for (int subsegmentIndex = 1; subsegmentIndex <= subsegmentCount; ++subsegmentIndex) {
        const double t = static_cast<double>(subsegmentIndex) / static_cast<double>(subsegmentCount);
        const HorizontalCoordinate coordinate =
            subsegmentIndex == subsegmentCount ? endCoordinate
                                               : SphericalGeometry::horizontalFromUnitVector(
                                                     interpolateUnitVector(startVector, endVector, angularDistance, t)
                                                 );
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
                segments, projection, previousCoordinate, coordinate, maxSegmentLengthSquared
            );
        }

        previousCoordinate = coordinate;
        hasPreviousCoordinate = true;
    }

    return segments;
}

}  // namespace skygate::core
