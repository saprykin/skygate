#pragma once

#include <chrono>

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

using UtcTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

struct EquatorialCoordinate {
    double rightAscensionHours = 0.0;
    double declinationDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
};

struct HorizontalCoordinate {
    static constexpr double kAltitudeMinDeg = -90.0;
    static constexpr double kAltitudeMaxDeg = 90.0;

    double altitudeDeg = 0.0;
    double azimuthDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] HorizontalCoordinate normalizedAzimuth() const noexcept;
};

struct SkyContext {
    GeoLocation observer;
    UtcTimePoint utcTime {};
};

}  // namespace skygate::core
