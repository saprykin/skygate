#include "skygate/core/math/LinePattern.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

std::vector<LineSegment2d> DashedLineBuilder::build(
    const LineSegment2d& segment,
    const double dashLength,
    const double gapLength
) const
{
    std::vector<LineSegment2d> segments;
    const double deltaX = segment.x2 - segment.x1;
    const double deltaY = segment.y2 - segment.y1;
    const double length = std::hypot(deltaX, deltaY);
    if (
        length <= 1e-9
        || !std::isfinite(length)
        || dashLength <= 0.0
        || gapLength < 0.0
    ) {
        return segments;
    }

    const double unitX = deltaX / length;
    const double unitY = deltaY / length;
    double dashStart = 0.0;
    while (dashStart < length) {
        const double dashEnd = std::min(dashStart + dashLength, length);
        segments.push_back(LineSegment2d {
            .x1 = segment.x1 + (unitX * dashStart),
            .y1 = segment.y1 + (unitY * dashStart),
            .x2 = segment.x1 + (unitX * dashEnd),
            .y2 = segment.y1 + (unitY * dashEnd)
        });
        dashStart += dashLength + gapLength;
    }

    return segments;
}

}  // namespace skygate::core
