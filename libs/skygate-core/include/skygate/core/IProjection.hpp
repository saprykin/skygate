#pragma once

#include "skygate/core/HorizontalCoordinate.hpp"
#include "skygate/core/ProjectionParams.hpp"
#include "skygate/core/ProjectionType.hpp"
#include "skygate/core/ScreenPoint.hpp"

namespace skygate::core {

class IProjection {
public:
    virtual ~IProjection() = default;

    [[nodiscard]] virtual ProjectionType type() const noexcept = 0;
    [[nodiscard]] virtual ScreenPoint project(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params
    ) const noexcept = 0;
};

}  // namespace skygate::core
