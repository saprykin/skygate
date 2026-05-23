#pragma once

#include "HorizontalCoordinate.hpp"
#include "ProjectionParams.hpp"
#include "ProjectionType.hpp"
#include "ScreenPoint.hpp"

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
