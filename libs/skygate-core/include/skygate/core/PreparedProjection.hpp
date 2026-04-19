#pragma once

#include "skygate/core/math/ProjectionMath.hpp"

#include <optional>

namespace skygate::core {

class PreparedProjection final {
public:
    [[nodiscard]] static std::optional<PreparedProjection> create(
        ProjectionType projectionType,
        const ProjectionParams& params
    ) noexcept;

    [[nodiscard]] ProjectionType type() const noexcept;
    [[nodiscard]] const ProjectionParams& params() const noexcept;
    [[nodiscard]] ScreenPoint project(const HorizontalCoordinate& coordinate) const noexcept;

private:
    ProjectionType m_projectionType = ProjectionType::Stereographic;
    ProjectionParams m_params;
    ProjectionMath::Vec3 m_center;
    ProjectionMath::Vec3 m_right;
    ProjectionMath::Vec3 m_up;
    double m_circularMaxRadius = 0.0;
    double m_rectHalfWidth = 0.0;
    double m_rectHalfHeight = 0.0;
};

}  // namespace skygate::core
