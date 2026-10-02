#pragma once

namespace skygate::core {

class PhysicalConstants final {
public:
    static constexpr double kSpeedOfLightKms = 299'792.458;
    static constexpr double kSpeedOfLightAuPerDay = 173.144632674240;
    static constexpr double kAstronomicalUnitMeters = 149'597'870'700.0;
    static constexpr double kAstronomicalUnitKilometers = 149'597'870.7;
    static constexpr double kAuPerParsec = 206'264.80624709636;
    static constexpr double kSolarSchwarzschildRadiusAu = 1.97412574336e-8;
    static constexpr double kSynodicMonthDays = 29.530588853;

    static constexpr double kWgs84EquatorialRadiusMeters = 6'378'137.0;
    static constexpr double kWgs84Flattening = 1.0 / 298.257223563;

    static constexpr double kSolarMeanAnomalyDegAtJ2000 = 357.528;
    static constexpr double kSolarMeanAnomalyDegPerDay = 0.9856003;
};

}  // namespace skygate::core
