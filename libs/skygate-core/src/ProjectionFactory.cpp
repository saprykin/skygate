#include "ProjectionFactory.hpp"
#include "projections/AzimuthalEquidistantProjection.hpp"
#include "projections/PerspectiveProjection.hpp"
#include "projections/StereographicProjection.hpp"

namespace skygate::core {

std::unique_ptr<IProjection> createProjection(const ProjectionType type)
{
    switch (type) {
    case ProjectionType::Stereographic:
        return std::make_unique<StereographicProjection>();
    case ProjectionType::AzimuthalEquidistant:
        return std::make_unique<AzimuthalEquidistantProjection>();
    case ProjectionType::Perspective:
        return std::make_unique<PerspectiveProjection>();
    }

    return nullptr;
}

}  // namespace skygate::core
