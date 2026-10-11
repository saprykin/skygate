#pragma once

namespace skygate::ephemeris {

// Altitude extremes of a fixed equatorial body for an observer. Both limits
// describe the diurnal range before refraction or topocentric parallax.
class FixedBodyAltitudeRange final {
public:
    FixedBodyAltitudeRange() = delete;

    [[nodiscard]] static double minAltitudeDeg(double observerLatitudeDeg, double declinationDeg) noexcept;
    [[nodiscard]] static double maxAltitudeDeg(double observerLatitudeDeg, double declinationDeg) noexcept;
};

}  // namespace skygate::ephemeris
