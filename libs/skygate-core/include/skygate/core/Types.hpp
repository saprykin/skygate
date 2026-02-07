#pragma once

#include <chrono>
#include <cstdint>

namespace skygate::core {

struct GeoLocation {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;

    bool isValid() const noexcept;
};

using UtcTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

struct EquatorialCoordinate {
    double rightAscensionHours = 0.0;
    double declinationDeg = 0.0;
};

struct HorizontalCoordinate {
    double altitudeDeg = 0.0;
    double azimuthDeg = 0.0;
};

struct SkyContext {
    GeoLocation observer;
    UtcTimePoint utcTime = std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::system_clock::now()
    );
};

enum class ProjectionType : std::uint8_t {
    Stereographic,
    AzimuthalEquidistant,
    Perspective
};

}  // namespace skygate::core
