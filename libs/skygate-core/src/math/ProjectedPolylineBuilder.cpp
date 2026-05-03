#include "skygate/core/math/ProjectedPolylineBuilder.hpp"

namespace skygate::core {

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

    bool hasPreviousPoint = false;
    ScreenPoint previousPoint;
    for (const HorizontalCoordinate& coordinate : coordinates) {
        const auto projected = projection.project(coordinate);
        if (!projected.isVisible || !projected.isFinite()) {
            hasPreviousPoint = false;
            continue;
        }

        if (hasPreviousPoint) {
            const double segmentLengthSquared = squaredDistance2d(
                projected.x,
                projected.y,
                previousPoint.x,
                previousPoint.y
            );
            if (segmentLengthSquared <= maxSegmentLengthSquared) {
                segments.push_back(LineSegment2d {
                    .x1 = previousPoint.x,
                    .y1 = previousPoint.y,
                    .x2 = projected.x,
                    .y2 = projected.y
                });
            }
        }

        previousPoint = projected;
        hasPreviousPoint = true;
    }

    return segments;
}

}  // namespace skygate::core
