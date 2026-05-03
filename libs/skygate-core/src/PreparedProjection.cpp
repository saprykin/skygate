#include "skygate/core/PreparedProjection.hpp"

#include "projections/ProjectionAlgorithms.hpp"

namespace skygate::core {

std::optional<PreparedProjection> PreparedProjection::create(
    const ProjectionType projectionType,
    const ProjectionParams& params
) noexcept
{
    ProjectionAlgorithms::ProjectionFrame frame;
    if (!ProjectionAlgorithms::prepareFrame(projectionType, params, frame)) {
        return std::nullopt;
    }

    PreparedProjection preparedProjection;
    preparedProjection.m_projectionType = frame.projectionType;
    preparedProjection.m_params = frame.params;
    preparedProjection.m_center = frame.center;
    preparedProjection.m_right = frame.right;
    preparedProjection.m_up = frame.up;
    preparedProjection.m_circularMaxRadius = frame.circularMaxRadius;
    preparedProjection.m_rectHalfWidth = frame.rectHalfWidth;
    preparedProjection.m_rectHalfHeight = frame.rectHalfHeight;
    return preparedProjection;
}

ProjectionType PreparedProjection::type() const noexcept
{
    return m_projectionType;
}

const ProjectionParams& PreparedProjection::params() const noexcept
{
    return m_params;
}

ScreenPoint PreparedProjection::project(const HorizontalCoordinate& coordinate) const noexcept
{
    ProjectionAlgorithms::ProjectionFrame frame;
    frame.projectionType = m_projectionType;
    frame.params = m_params;
    frame.center = m_center;
    frame.right = m_right;
    frame.up = m_up;
    frame.circularMaxRadius = m_circularMaxRadius;
    frame.rectHalfWidth = m_rectHalfWidth;
    frame.rectHalfHeight = m_rectHalfHeight;
    return ProjectionAlgorithms::project(frame, coordinate);
}

}  // namespace skygate::core
