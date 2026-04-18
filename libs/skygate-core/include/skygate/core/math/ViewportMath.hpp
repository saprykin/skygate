#pragma once

#include "skygate/core/ProjectionTypes.hpp"

namespace skygate::core {

class ViewportMath final {
public:
    static constexpr double kDefaultCenterAltitudeDeg = 45.0;
    static constexpr double kDefaultCenterAzimuthDeg = 180.0;
    static constexpr double kAltitudeMinDeg = HorizontalCoordinate::kAltitudeMinDeg;
    static constexpr double kAltitudeMaxDeg = HorizontalCoordinate::kAltitudeMaxDeg;
    static constexpr double kFieldOfViewMinDeg = ProjectionParams::kFieldOfViewMinDeg;
    static constexpr double kFieldOfViewMaxDeg = ProjectionParams::kFieldOfViewMaxDeg;

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
