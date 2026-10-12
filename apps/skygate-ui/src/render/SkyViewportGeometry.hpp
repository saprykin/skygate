#pragma once

#include "SkyViewportGeometryInput.hpp"
#include "SkyViewportLineSegment.hpp"

#include <vector>

namespace skygate::ui::internal {

[[nodiscard]] std::vector<SkyViewportLineSegment> buildSkyViewportLineSegments(const SkyViewportGeometryInput& input);

}  // namespace skygate::ui::internal
