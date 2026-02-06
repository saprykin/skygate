#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace skygate::ephemeris {
namespace {

constexpr double kJ2000JulianDay = 2451545.0;
constexpr double kUnixEpochJulianDay = 2440587.5;
constexpr double kSecondsPerDay = 86400.0;

struct FixedStarCoordinate {
    const char* id;
    core::EquatorialCoordinate equatorial;
};

[[nodiscard]] double degreesToRadians(const double degrees)
{
    return degrees * std::numbers::pi_v<double> / 180.0;
}

[[nodiscard]] double radiansToDegrees(const double radians)
{
    return radians * 180.0 / std::numbers::pi_v<double>;
}

[[nodiscard]] double normalizeDegrees(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }

    return degrees;
}

[[nodiscard]] double normalizeHours(double hours)
{
    hours = std::fmod(hours, 24.0);
    if (hours < 0.0) {
        hours += 24.0;
    }

    return hours;
}

[[nodiscard]] double normalizeDegreesSigned(double degrees)
{
    degrees = normalizeDegrees(degrees);
    if (degrees > 180.0) {
        degrees -= 360.0;
    }

    return degrees;
}

[[nodiscard]] double julianDayFromUtc(const core::UtcTimePoint& utcTime)
{
    const auto epochSeconds = utcTime.time_since_epoch().count();
    return static_cast<double>(epochSeconds) / kSecondsPerDay + kUnixEpochJulianDay;
}

[[nodiscard]] double meanObliquityDeg(const double daysSinceJ2000)
{
    return 23.4393 - 0.0000004 * daysSinceJ2000;
}

[[nodiscard]] core::EquatorialCoordinate eclipticToEquatorial(
    const double eclipticLongitudeDeg,
    const double eclipticLatitudeDeg,
    const double obliquityDeg
)
{
    const double lonRad = degreesToRadians(eclipticLongitudeDeg);
    const double latRad = degreesToRadians(eclipticLatitudeDeg);
    const double epsRad = degreesToRadians(obliquityDeg);

    const double xEcl = std::cos(lonRad) * std::cos(latRad);
    const double yEcl = std::sin(lonRad) * std::cos(latRad);
    const double zEcl = std::sin(latRad);

    const double xEq = xEcl;
    const double yEq = yEcl * std::cos(epsRad) - zEcl * std::sin(epsRad);
    const double zEq = yEcl * std::sin(epsRad) + zEcl * std::cos(epsRad);

    core::EquatorialCoordinate equatorial;
    equatorial.rightAscensionHours = normalizeHours(radiansToDegrees(std::atan2(yEq, xEq)) / 15.0);
    equatorial.declinationDeg = radiansToDegrees(std::asin(zEq));
    return equatorial;
}

[[nodiscard]] core::EquatorialCoordinate computeSunEquatorial(const core::UtcTimePoint& utcTime)
{
    const double julianDay = julianDayFromUtc(utcTime);
    const double daysSinceJ2000 = julianDay - kJ2000JulianDay;

    const double meanLongitudeDeg = normalizeDegrees(280.460 + 0.9856474 * daysSinceJ2000);
    const double meanAnomalyDeg = normalizeDegrees(357.528 + 0.9856003 * daysSinceJ2000);
    const double meanAnomalyRad = degreesToRadians(meanAnomalyDeg);

    const double eclipticLongitudeDeg = normalizeDegrees(
        meanLongitudeDeg + 1.915 * std::sin(meanAnomalyRad) + 0.020 * std::sin(2.0 * meanAnomalyRad)
    );

    return eclipticToEquatorial(
        eclipticLongitudeDeg,
        0.0,
        meanObliquityDeg(daysSinceJ2000)
    );
}

[[nodiscard]] core::EquatorialCoordinate computeMoonEquatorial(const core::UtcTimePoint& utcTime)
{
    const double julianDay = julianDayFromUtc(utcTime);
    const double daysSinceJ2000 = julianDay - kJ2000JulianDay;

    const double ascendingNodeDeg = normalizeDegrees(125.1228 - 0.0529538083 * daysSinceJ2000);
    const double inclinationDeg = 5.1454;
    const double argumentOfPerigeeDeg = normalizeDegrees(318.0634 + 0.1643573223 * daysSinceJ2000);
    const double eccentricity = 0.0549;
    const double meanAnomalyDeg = normalizeDegrees(115.3654 + 13.0649929509 * daysSinceJ2000);

    const double meanAnomalyRad = degreesToRadians(meanAnomalyDeg);
    const double eccentricAnomalyDeg = meanAnomalyDeg +
        radiansToDegrees(eccentricity * std::sin(meanAnomalyRad) * (1.0 + eccentricity * std::cos(meanAnomalyRad)));
    const double eccentricAnomalyRad = degreesToRadians(eccentricAnomalyDeg);

    constexpr double semiMajorAxisEarthRadii = 60.2666;
    const double xv = semiMajorAxisEarthRadii * (std::cos(eccentricAnomalyRad) - eccentricity);
    const double yv = semiMajorAxisEarthRadii * (std::sqrt(1.0 - eccentricity * eccentricity) * std::sin(eccentricAnomalyRad));

    const double trueAnomalyDeg = radiansToDegrees(std::atan2(yv, xv));
    const double radius = std::sqrt(xv * xv + yv * yv);

    const double longitudeArgumentRad = degreesToRadians(trueAnomalyDeg + argumentOfPerigeeDeg);
    const double ascendingNodeRad = degreesToRadians(ascendingNodeDeg);
    const double inclinationRad = degreesToRadians(inclinationDeg);

    const double xH = radius * (
        std::cos(ascendingNodeRad) * std::cos(longitudeArgumentRad) -
        std::sin(ascendingNodeRad) * std::sin(longitudeArgumentRad) * std::cos(inclinationRad)
    );
    const double yH = radius * (
        std::sin(ascendingNodeRad) * std::cos(longitudeArgumentRad) +
        std::cos(ascendingNodeRad) * std::sin(longitudeArgumentRad) * std::cos(inclinationRad)
    );
    const double zH = radius * std::sin(longitudeArgumentRad) * std::sin(inclinationRad);

    const double eclipticLongitudeDeg = normalizeDegrees(radiansToDegrees(std::atan2(yH, xH)));
    const double eclipticLatitudeDeg = radiansToDegrees(std::atan2(zH, std::sqrt(xH * xH + yH * yH)));

    return eclipticToEquatorial(
        eclipticLongitudeDeg,
        eclipticLatitudeDeg,
        meanObliquityDeg(daysSinceJ2000)
    );
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> fixedStarEquatorialById(const std::string& id)
{
    constexpr std::array<FixedStarCoordinate, 8> kFixedStarCoordinates = {{
        {.id = "sirius", .equatorial = {.rightAscensionHours = 6.7525, .declinationDeg = -16.7161}},
        {.id = "canopus", .equatorial = {.rightAscensionHours = 6.3992, .declinationDeg = -52.6957}},
        {.id = "arcturus", .equatorial = {.rightAscensionHours = 14.2610, .declinationDeg = 19.1825}},
        {.id = "vega", .equatorial = {.rightAscensionHours = 18.6156, .declinationDeg = 38.7837}},
        {.id = "capella", .equatorial = {.rightAscensionHours = 5.2782, .declinationDeg = 45.9979}},
        {.id = "rigel", .equatorial = {.rightAscensionHours = 5.2423, .declinationDeg = -8.2016}},
        {.id = "procyon", .equatorial = {.rightAscensionHours = 7.6550, .declinationDeg = 5.2250}},
        {.id = "betelgeuse", .equatorial = {.rightAscensionHours = 5.9195, .declinationDeg = 7.4071}},
    }};

    for (const auto& star : kFixedStarCoordinates) {
        if (id == star.id) {
            return star.equatorial;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> computeEquatorial(
    const CelestialBody& body,
    const core::UtcTimePoint& utcTime
)
{
    if (body.type == CelestialBodyType::Sun || body.id == "sun") {
        return computeSunEquatorial(utcTime);
    }

    if (body.type == CelestialBodyType::Moon || body.id == "moon") {
        return computeMoonEquatorial(utcTime);
    }

    if (body.type == CelestialBodyType::Star) {
        return fixedStarEquatorialById(body.id);
    }

    return std::nullopt;
}

[[nodiscard]] core::HorizontalCoordinate equatorialToHorizontal(
    const core::EquatorialCoordinate& equatorial,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
)
{
    const double julianDay = julianDayFromUtc(utcTime);
    const double gmstDeg = normalizeDegrees(280.46061837 + 360.98564736629 * (julianDay - kJ2000JulianDay));
    const double localSiderealDeg = normalizeDegrees(gmstDeg + observer.longitudeDeg);

    const double hourAngleDeg = normalizeDegreesSigned(localSiderealDeg - equatorial.rightAscensionHours * 15.0);

    const double hourAngleRad = degreesToRadians(hourAngleDeg);
    const double declinationRad = degreesToRadians(equatorial.declinationDeg);
    const double latitudeRad = degreesToRadians(observer.latitudeDeg);

    const double x = std::cos(declinationRad) * std::cos(hourAngleRad);
    const double y = std::cos(declinationRad) * std::sin(hourAngleRad);
    const double z = std::sin(declinationRad);

    const double xHor = x * std::sin(latitudeRad) - z * std::cos(latitudeRad);
    const double yHor = y;
    const double zHor = x * std::cos(latitudeRad) + z * std::sin(latitudeRad);

    core::HorizontalCoordinate horizontal;
    horizontal.altitudeDeg = radiansToDegrees(std::asin(zHor));
    horizontal.azimuthDeg = normalizeDegrees(radiansToDegrees(std::atan2(yHor, xHor)) + 180.0);
    return horizontal;
}

}  // namespace

class EphemerisEngineStub final : public IEphemerisEngine {
public:
    explicit EphemerisEngineStub(const IStarCatalog* catalog)
        : m_catalog(catalog)
    {
    }

    [[nodiscard]] SkySnapshot compute(const core::SkyContext& context) const override
    {
        SkySnapshot snapshot;
        snapshot.context = context;

        if (m_catalog == nullptr) {
            return snapshot;
        }

        const auto bodies = m_catalog->bodies();
        snapshot.states.reserve(bodies.size());

        for (const auto& body : bodies) {
            CelestialBodyState state;
            state.body = body;

            if (const auto equatorial = computeEquatorial(body, context.utcTime); equatorial.has_value()) {
                state.equatorial = *equatorial;
                if (context.observer.isValid()) {
                    state.horizontal = equatorialToHorizontal(*equatorial, context.observer, context.utcTime);
                }
            }

            snapshot.states.push_back(state);
        }

        return snapshot;
    }

private:
    const IStarCatalog* m_catalog = nullptr;
};

std::unique_ptr<IEphemerisEngine> createEphemerisEngineStub(const IStarCatalog* catalog)
{
    return std::make_unique<EphemerisEngineStub>(catalog);
}

}  // namespace skygate::ephemeris
