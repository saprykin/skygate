#pragma once

#include "skygate/core/IProjection.hpp"

#include <memory>

namespace skygate::core {

[[nodiscard]] std::unique_ptr<IProjection> createProjection(ProjectionType type);

}  // namespace skygate::core
