#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/core/projections/PerspectiveProjection.hpp"
#include "skygate/core/projections/StereographicProjection.hpp"

namespace skygate::core {

std::unique_ptr<IProjection> createProjection(const ProjectionType type)
{
    switch (type) {
    case ProjectionType::Stereographic:
        return std::make_unique<StereographicProjection>();
    case ProjectionType::AzimuthalEquidistant:
        return nullptr;
    case ProjectionType::Perspective:
        return std::make_unique<PerspectiveProjection>();
    }

    return nullptr;
}

}  // namespace skygate::core
