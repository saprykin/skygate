#pragma once

#include "math/LineSegment2d.hpp"

#include <vector>

namespace skygate::core {

class DashedLineBuilder final {
public:
    [[nodiscard]] std::vector<LineSegment2d>
    build(const LineSegment2d& segment, double dashLength, double gapLength) const;
};

}  // namespace skygate::core
