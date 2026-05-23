#pragma once

#include "PreparedProjection.hpp"
#include "math/LineSegment2d.hpp"

#include <span>
#include <vector>

namespace skygate::core {

class ProjectedPolylineBuilder final {
public:
    [[nodiscard]] std::vector<LineSegment2d> build(
        const PreparedProjection& projection,
        std::span<const HorizontalCoordinate> coordinates,
        double maxSegmentLengthSquared
    ) const;
};

}  // namespace skygate::core
