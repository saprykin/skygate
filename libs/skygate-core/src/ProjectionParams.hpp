#pragma once

#include "HorizontalCoordinate.hpp"

namespace skygate::core {

struct ProjectionParams {
    static constexpr double kFieldOfViewMinDeg = 1.0;
    static constexpr double kFieldOfViewMaxDeg = 150.0;

    HorizontalCoordinate center;
    double fovDeg = 90.0;
    double rollDeg = 0.0;
    double viewportWidth = 1.0;
    double viewportHeight = 1.0;

    [[nodiscard]] bool isProjectable() const noexcept;
};

}  // namespace skygate::core
