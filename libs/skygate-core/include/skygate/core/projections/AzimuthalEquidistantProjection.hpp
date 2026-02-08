#pragma once

#include "skygate/core/IProjection.hpp"

namespace skygate::core {

class AzimuthalEquidistantProjection final : public IProjection {
public:
    [[nodiscard]] ProjectionType type() const noexcept override;
    [[nodiscard]] ScreenPoint project(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params
    ) const noexcept override;
};

}  // namespace skygate::core
