#include "skygate/core/projections/AzimuthalEquidistantProjection.hpp"

#include "ProjectionAlgorithms.hpp"

namespace skygate::core {

ProjectionType AzimuthalEquidistantProjection::type() const noexcept
{
    return ProjectionType::AzimuthalEquidistant;
}

ScreenPoint AzimuthalEquidistantProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    return ProjectionAlgorithms::project(type(), coordinate, params);
}

}  // namespace skygate::core
