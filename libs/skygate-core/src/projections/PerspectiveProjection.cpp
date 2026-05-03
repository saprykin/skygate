#include "skygate/core/projections/PerspectiveProjection.hpp"

#include "ProjectionAlgorithms.hpp"

namespace skygate::core {

ProjectionType PerspectiveProjection::type() const noexcept
{
    return ProjectionType::Perspective;
}

ScreenPoint PerspectiveProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    return ProjectionAlgorithms::project(type(), coordinate, params);
}

}  // namespace skygate::core
