#pragma once

#include "LineSegment2d.hpp"
#include "PreparedProjection.hpp"

#include <span>
#include <vector>

namespace skygate::core {

class ProjectedPolylineBuilder final {
public:
    // Squared pixel length above which a projected chord counts as a wrap and
    // is dropped. The fraction is applied to the larger viewport dimension.
    [[nodiscard]] static double
    viewportSegmentLimitSquared(double viewportWidth, double viewportHeight, double viewportFraction) noexcept;

    [[nodiscard]] std::vector<LineSegment2d> build(
        const PreparedProjection& projection,
        std::span<const HorizontalCoordinate> coordinates,
        double maxSegmentLengthSquared
    ) const;
};

}  // namespace skygate::core
