#include "PreparedProjection.hpp"
#include "projections/ProjectionAlgorithms.hpp"

namespace skygate::core {

std::optional<PreparedProjection>
PreparedProjection::create(const ProjectionType projectionType, const ProjectionParams& params) noexcept
{
    ProjectionFrame frame;
    if (!ProjectionAlgorithms::prepareFrame(projectionType, params, frame)) {
        return std::nullopt;
    }

    PreparedProjection preparedProjection;
    preparedProjection.m_frame = frame;
    return preparedProjection;
}

ProjectionType PreparedProjection::type() const noexcept
{
    return m_frame.projectionType;
}

const ProjectionParams& PreparedProjection::params() const noexcept
{
    return m_frame.params;
}

ScreenPoint PreparedProjection::project(const HorizontalCoordinate& coordinate) const noexcept
{
    return projectWithMargin(coordinate, 0.0);
}

ScreenPoint
PreparedProjection::projectWithMargin(const HorizontalCoordinate& coordinate, const double marginPx) const noexcept
{
    return ProjectionAlgorithms::project(m_frame, coordinate, marginPx);
}

}  // namespace skygate::core
