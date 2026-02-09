#pragma once

#include "skygate/core/IProjection.hpp"

namespace skygate::core {

class ViewportMath final {
public:
    static constexpr double kDefaultCenterAltitudeDeg = 45.0;
    static constexpr double kDefaultCenterAzimuthDeg = 180.0;
    static constexpr double kAltitudeMinDeg = -90.0;
    static constexpr double kAltitudeMaxDeg = 90.0;
    static constexpr double kFieldOfViewMinDeg = 20.0;
    static constexpr double kFieldOfViewMaxDeg = 150.0;

    [[nodiscard]] static double normalizeAzimuthDeg(double azimuthDeg) noexcept;
    [[nodiscard]] static double clampAltitudeDeg(double altitudeDeg) noexcept;
    [[nodiscard]] static double clampFieldOfViewDeg(double fieldOfViewDeg) noexcept;

    [[nodiscard]] static ProjectionParams buildProjectionParams(
        double viewportWidth,
        double viewportHeight,
        double centerAltitudeDeg,
        double centerAzimuthDeg,
        double fieldOfViewDeg
    ) noexcept;
};

}  // namespace skygate::core
