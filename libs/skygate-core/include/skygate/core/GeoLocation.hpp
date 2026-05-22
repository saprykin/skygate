#pragma once

namespace skygate::core {

struct GeoLocation {
    static constexpr double kLatitudeMinDeg = -90.0;
    static constexpr double kLatitudeMaxDeg = 90.0;
    static constexpr double kLongitudeMinDeg = -180.0;
    static constexpr double kLongitudeMaxDeg = 180.0;

    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;

    [[nodiscard]] bool isValid() const noexcept;
};

}  // namespace skygate::core
