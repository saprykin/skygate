#pragma once

#include "ProjectionType.hpp"
#include "SkyViewState.hpp"

namespace skygate::ui::internal {

class SkyViewController final {
public:
    [[nodiscard]] double magnitudeCutoff() const noexcept;
    [[nodiscard]] double centerAltitudeDeg() const noexcept;
    [[nodiscard]] double centerAzimuthDeg() const noexcept;
    [[nodiscard]] double fieldOfViewDeg() const noexcept;
    [[nodiscard]] skygate::core::ProjectionType projectionType() const noexcept;

    [[nodiscard]] bool setMagnitudeCutoff(double magnitudeCutoff) noexcept;
    [[nodiscard]] bool setCenter(double altitudeDeg, double azimuthDeg) noexcept;
    [[nodiscard]] bool setFieldOfViewDeg(double fieldOfViewDeg) noexcept;
    [[nodiscard]] bool setProjectionType(skygate::core::ProjectionType projectionType);

private:
    SkyViewState m_state;
};

}  // namespace skygate::ui::internal
