#pragma once

#include "ProjectionStatus.hpp"

namespace skygate::core {

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
    ProjectionStatus status = ProjectionStatus::Culled;

    [[nodiscard]] bool isVisible() const noexcept;
    [[nodiscard]] bool isFinite() const noexcept;
};

}  // namespace skygate::core
