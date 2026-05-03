#include "skygate/core/projections/StereographicProjection.hpp"

#include "ProjectionAlgorithms.hpp"

namespace skygate::core {

ProjectionType StereographicProjection::type() const noexcept
{
    return ProjectionType::Stereographic;
}

ScreenPoint StereographicProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    return ProjectionAlgorithms::project(type(), coordinate, params);
}

}  // namespace skygate::core
