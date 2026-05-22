#pragma once

namespace skygate::core {

struct HorizontalCoordinate {
    static constexpr double kAltitudeMinDeg = -90.0;
    static constexpr double kAltitudeMaxDeg = 90.0;

    double altitudeDeg = 0.0;
    double azimuthDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] HorizontalCoordinate normalizedAzimuth() const noexcept;
};

}  // namespace skygate::core
