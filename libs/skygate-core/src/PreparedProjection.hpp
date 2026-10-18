#pragma once

#include "ProjectionParams.hpp"
#include "ProjectionType.hpp"
#include "ScreenPoint.hpp"
#include "projections/ProjectionFrame.hpp"

#include <optional>

namespace skygate::core {

class PreparedProjection final {
public:
    [[nodiscard]] static std::optional<PreparedProjection>
    create(ProjectionType projectionType, const ProjectionParams& params) noexcept;

    [[nodiscard]] ProjectionType type() const noexcept;
    [[nodiscard]] const ProjectionParams& params() const noexcept;
    [[nodiscard]] ScreenPoint project(const HorizontalCoordinate& coordinate) const noexcept;
    [[nodiscard]] ScreenPoint projectWithMargin(const HorizontalCoordinate& coordinate, double marginPx) const noexcept;

private:
    ProjectionFrame m_frame;
};

}  // namespace skygate::core
