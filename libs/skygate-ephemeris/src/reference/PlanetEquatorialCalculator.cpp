#include "PlanetEquatorialCalculator.hpp"
#include "EclipticToEquatorialCalculator.hpp"
#include "PlanetIdentity.hpp"
#include "math/AngleMath.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"

#include <array>
#include <cmath>

namespace skygate::ephemeris {
namespace {

struct PlanetApproximateOrbit {
    PlanetIdentity::Key key;
    double meanLongitudeDegAtJ2000;
    double meanMotionDegPerDay;
    double latitudeAmplitudeDeg;
    double latitudePhaseDeg;
};

constexpr std::array<PlanetApproximateOrbit, 7> kPlanetApproximateOrbits = {{
    {.key = PlanetIdentity::Key::Mercury,
     .meanLongitudeDegAtJ2000 = 252.25084,
     .meanMotionDegPerDay = 4.09233445,
     .latitudeAmplitudeDeg = 2.2,
     .latitudePhaseDeg = 20.0},
    {.key = PlanetIdentity::Key::Venus,
     .meanLongitudeDegAtJ2000 = 181.97973,
     .meanMotionDegPerDay = 1.60213034,
     .latitudeAmplitudeDeg = 1.6,
     .latitudePhaseDeg = 60.0},
    {.key = PlanetIdentity::Key::Mars,
     .meanLongitudeDegAtJ2000 = 355.43300,
     .meanMotionDegPerDay = 0.52402068,
     .latitudeAmplitudeDeg = 1.8,
     .latitudePhaseDeg = 140.0},
    {.key = PlanetIdentity::Key::Jupiter,
     .meanLongitudeDegAtJ2000 = 34.35152,
     .meanMotionDegPerDay = 0.08308529,
     .latitudeAmplitudeDeg = 1.1,
     .latitudePhaseDeg = 210.0},
    {.key = PlanetIdentity::Key::Saturn,
     .meanLongitudeDegAtJ2000 = 50.07744,
     .meanMotionDegPerDay = 0.03344414,
     .latitudeAmplitudeDeg = 1.0,
     .latitudePhaseDeg = 280.0},
    {.key = PlanetIdentity::Key::Uranus,
     .meanLongitudeDegAtJ2000 = 314.05501,
     .meanMotionDegPerDay = 0.01172834,
     .latitudeAmplitudeDeg = 0.8,
     .latitudePhaseDeg = 325.0},
    {.key = PlanetIdentity::Key::Neptune,
     .meanLongitudeDegAtJ2000 = 304.34866,
     .meanMotionDegPerDay = 0.00598103,
     .latitudeAmplitudeDeg = 0.7,
     .latitudePhaseDeg = 35.0},
}};

}  // namespace

std::optional<skygate::core::EquatorialCoordinate> PlanetEquatorialCalculator::compute(
    const std::string_view bodyId, const skygate::core::UtcTimePoint& utcTime
) const noexcept
{
    const std::optional<PlanetIdentity> planet = PlanetIdentity::fromBodyId(bodyId);
    if (!planet.has_value()) {
        return std::nullopt;
    }

    const double daysSinceJ2000 = skygate::core::EpochCodec::daysSinceJ2000(utcTime);
    const double obliquityDeg = skygate::core::AstronomicalTime::meanObliquityDeg(daysSinceJ2000);

    for (const PlanetApproximateOrbit& orbit : kPlanetApproximateOrbits) {
        if (planet->key() != orbit.key) {
            continue;
        }

        const double eclipticLongitudeDeg = skygate::core::AngleMath::normalizeDegrees(
            orbit.meanLongitudeDegAtJ2000 + orbit.meanMotionDegPerDay * daysSinceJ2000
        );
        const double eclipticLatitudeDeg =
            orbit.latitudeAmplitudeDeg
            * std::sin(skygate::core::AngleMath::toRadians(eclipticLongitudeDeg + orbit.latitudePhaseDeg));

        return EclipticToEquatorialCalculator::compute(eclipticLongitudeDeg, eclipticLatitudeDeg, obliquityDeg);
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
