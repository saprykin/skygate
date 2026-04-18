#pragma once

#include "skygate/core/SkyTypes.hpp"

#include <cstdint>

namespace skygate::core {

enum class ProjectionType : std::uint8_t {
    Stereographic,
    AzimuthalEquidistant,
    Perspective
};

enum class ProjectionStatus : std::uint8_t {
    Culled,
    Visible,
    InvalidCoordinate,
    InvalidParameters
};

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
    bool isVisible = false;
    ProjectionStatus status = ProjectionStatus::Culled;
};

struct ProjectionParams {
    static constexpr double kFieldOfViewMinDeg = 20.0;
    static constexpr double kFieldOfViewMaxDeg = 150.0;

    HorizontalCoordinate center;
    double fovDeg = 90.0;
    double rollDeg = 0.0;
    double viewportWidth = 1.0;
    double viewportHeight = 1.0;

    [[nodiscard]] bool isProjectable() const noexcept;
};

}  // namespace skygate::core
