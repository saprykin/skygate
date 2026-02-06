#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::core {

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
    bool isVisible = false;
};

struct ProjectionParams {
    HorizontalCoordinate center;
    double fovDeg = 90.0;
    double rollDeg = 0.0;
    double viewportWidth = 1.0;
    double viewportHeight = 1.0;
};

class IProjection {
public:
    virtual ~IProjection() = default;

    [[nodiscard]] virtual ProjectionType type() const noexcept = 0;
    [[nodiscard]] virtual ScreenPoint project(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params
    ) const noexcept = 0;
};

}  // namespace skygate::core
