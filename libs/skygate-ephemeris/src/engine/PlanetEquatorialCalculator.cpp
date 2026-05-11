#include "engine/PlanetEquatorialCalculator.hpp"

#include "engine/AstronomicalTime.hpp"
#include "engine/EclipticToEquatorialCalculator.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <array>
#include <cmath>

namespace skygate::ephemeris {
namespace {

struct PlanetApproximateOrbit {
    const char* id;
    double meanLongitudeDegAtJ2000;
    double meanMotionDegPerDay;
    double latitudeAmplitudeDeg;
    double latitudePhaseDeg;
};

constexpr std::array<PlanetApproximateOrbit, 7> kPlanetApproximateOrbits = {{
    {.id = "mercury", .meanLongitudeDegAtJ2000 = 252.25084, .meanMotionDegPerDay = 4.09233445, .latitudeAmplitudeDeg = 2.2, .latitudePhaseDeg = 20.0},
    {.id = "venus", .meanLongitudeDegAtJ2000 = 181.97973, .meanMotionDegPerDay = 1.60213034, .latitudeAmplitudeDeg = 1.6, .latitudePhaseDeg = 60.0},
    {.id = "mars", .meanLongitudeDegAtJ2000 = 355.43300, .meanMotionDegPerDay = 0.52402068, .latitudeAmplitudeDeg = 1.8, .latitudePhaseDeg = 140.0},
    {.id = "jupiter", .meanLongitudeDegAtJ2000 = 34.35152, .meanMotionDegPerDay = 0.08308529, .latitudeAmplitudeDeg = 1.1, .latitudePhaseDeg = 210.0},
    {.id = "saturn", .meanLongitudeDegAtJ2000 = 50.07744, .meanMotionDegPerDay = 0.03344414, .latitudeAmplitudeDeg = 1.0, .latitudePhaseDeg = 280.0},
    {.id = "uranus", .meanLongitudeDegAtJ2000 = 314.05501, .meanMotionDegPerDay = 0.01172834, .latitudeAmplitudeDeg = 0.8, .latitudePhaseDeg = 325.0},
    {.id = "neptune", .meanLongitudeDegAtJ2000 = 304.34866, .meanMotionDegPerDay = 0.00598103, .latitudeAmplitudeDeg = 0.7, .latitudePhaseDeg = 35.0},
}};

}  // namespace

std::optional<core::EquatorialCoordinate> PlanetEquatorialCalculator::compute(
    const std::string_view bodyId,
    const core::UtcTimePoint& utcTime
) const noexcept
{
    const double daysSinceJ2000 = AstronomicalTime::daysSinceJ2000(utcTime);
    const double obliquityDeg = AstronomicalTime::meanObliquityDeg(daysSinceJ2000);

    for (const PlanetApproximateOrbit& planet : kPlanetApproximateOrbits) {
        if (bodyId != planet.id) {
            continue;
        }

        const double eclipticLongitudeDeg = core::AngleMath::normalizeDegrees(
            planet.meanLongitudeDegAtJ2000 + planet.meanMotionDegPerDay * daysSinceJ2000
        );
        const double eclipticLatitudeDeg = planet.latitudeAmplitudeDeg * std::sin(
            core::AngleMath::toRadians(eclipticLongitudeDeg + planet.latitudePhaseDeg)
        );

        return EclipticToEquatorialCalculator::compute(
            eclipticLongitudeDeg,
            eclipticLatitudeDeg,
            obliquityDeg
        );
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
