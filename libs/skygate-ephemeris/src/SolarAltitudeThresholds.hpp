#pragma once

namespace skygate::ephemeris {

// Solar altitude crossings that define the standard twilight phases. Both the
// night-conditions calculator and presentation code share these values so the
// phase boundaries stay consistent.
class SolarAltitudeThresholds final {
public:
    SolarAltitudeThresholds() = delete;

    static constexpr double kSunriseSunsetAltitudeDeg = -0.833;
    static constexpr double kCivilTwilightAltitudeDeg = -6.0;
    static constexpr double kNauticalTwilightAltitudeDeg = -12.0;
    static constexpr double kAstronomicalTwilightAltitudeDeg = -18.0;
};

}  // namespace skygate::ephemeris
