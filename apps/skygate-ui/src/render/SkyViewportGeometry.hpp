#pragma once

#include "SkyViewportGeometryInput.hpp"
#include "SkyViewportLineSegment.hpp"

#include <vector>

namespace skygate::ui::internal {

class SkyViewportGeometry final {
public:
    [[nodiscard]] static std::vector<SkyViewportLineSegment> buildLineSegments(const SkyViewportGeometryInput& input);
};

}  // namespace skygate::ui::internal
