#include "PerformanceGuardScenario.hpp"

#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ui::tests {

namespace {

skygate::ephemeris::DistantCelestialBody makeDeepSkyBody(
    std::string id,
    std::string displayName,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg
    };
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases =
            {"Guard Alias " + body.id,
             "Shared Collision Alias "
                 + std::to_string(static_cast<int>(std::fmod(rightAscensionHours * 1000.0, 25.0)))},
        .majorAxisArcmin = 5.0,
        .minorAxisArcmin = 3.0,
        .positionAngleDeg = 0.0
    };
    return body;
}

skygate::ephemeris::OwnGalaxyCelestialBody makeHighPrecisionGuardBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body = PerformanceGuardScenario::makeBody(
        std::move(id), std::move(displayName), type, visualMagnitude, rightAscensionHours, declinationDeg
    );
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    return body;
}

skygate::ephemeris::OwnGalaxyCelestialBody makeHighPrecisionMoonBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "moon";
    body.displayName = "Moon";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Moon;
    body.visualMagnitude = -12.0;
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Moon;
    return body;
}

}  // namespace

SkyContextController::InitializationOptions PerformanceGuardScenario::initializationOptions()
{
    return SkyContextController::InitializationOptions{.loadSettings = false, .initializeLocation = false};
}

skygate::ephemeris::OwnGalaxyCelestialBody PerformanceGuardScenario::makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg
    };
    return body;
}

skygate::ephemeris::CelestialBodyCatalog PerformanceGuardScenario::makeLargeMixedCatalog(const bool includeExactTarget)
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies;
    std::vector<skygate::ephemeris::DistantCelestialBody> deepSkyBodies;
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    bodies.reserve(7000U);
    deepSkyBodies.reserve(2000U);
    orderedBodyIndexes.reserve(9000U);
    for (int index = 0; index < 7000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.017, 24.0);
        const double declinationDeg = -72.0 + static_cast<double>(index % 145);
        const double magnitude = 2.0 + static_cast<double>(index % 80) / 10.0;
        orderedBodyIndexes.push_back(
            {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = bodies.size()}
        );
        bodies.push_back(makeBody(
            "guard_star_" + std::to_string(index),
            "Guard Star " + std::to_string(index),
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }
    if (includeExactTarget) {
        orderedBodyIndexes.push_back(
            {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = bodies.size()}
        );
        bodies.push_back(makeBody(
            "guard_exact_target",
            "Guard Exact Target",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            -1.0,
            6.0,
            -16.0
        ));
    }

    for (int index = 0; index < 2000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.071, 24.0);
        const double declinationDeg = -55.0 + static_cast<double>(index % 110);
        const double magnitude = 4.0 + static_cast<double>(index % 90) / 10.0;
        orderedBodyIndexes.push_back(
            {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = deepSkyBodies.size()}
        );
        deepSkyBodies.push_back(makeDeepSkyBody(
            "guard_dso_" + std::to_string(index),
            "Guard Galaxy " + std::to_string(index),
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }
    return skygate::ephemeris::CelestialBodyCatalog(
        std::move(bodies), std::move(deepSkyBodies), std::move(orderedBodyIndexes)
    );
}

std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> PerformanceGuardScenario::makeHighPrecisionGuardCatalog()
{
    constexpr int kHygScaleStarCount = 119626;
    constexpr int kOpenNgcScaleDeepSkyCount = 13308;

    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies;
    bodies.reserve(static_cast<std::size_t>(kHygScaleStarCount + kOpenNgcScaleDeepSkyCount));
    for (int index = 0; index < kHygScaleStarCount; ++index) {
        bodies.push_back(makeHighPrecisionGuardBody(
            "hp_guard_star_" + std::to_string(index),
            "HP Guard Star " + std::to_string(index),
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            2.0 + static_cast<double>(index % 100) / 10.0,
            std::fmod(static_cast<double>(index) * 0.011, 24.0),
            -75.0 + static_cast<double>(index % 150)
        ));
    }
    for (int index = 0; index < kOpenNgcScaleDeepSkyCount; ++index) {
        bodies.push_back(makeHighPrecisionGuardBody(
            "hp_guard_dso_" + std::to_string(index),
            "HP Guard Galaxy " + std::to_string(index),
            skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject,
            4.0 + static_cast<double>(index % 80) / 10.0,
            std::fmod(static_cast<double>(index) * 0.073, 24.0),
            -55.0 + static_cast<double>(index % 110)
        ));
    }
    return bodies;
}

std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> PerformanceGuardScenario::makeHighPrecisionMoonGuardCatalog()
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies = makeHighPrecisionGuardCatalog();
    bodies.insert(bodies.begin(), makeHighPrecisionMoonBody());
    return bodies;
}

skygate::ephemeris::EphemerisDatasetInfo PerformanceGuardScenario::makeHighPrecisionGuardDataSetInfo()
{
    skygate::ephemeris::EphemerisDatasetInfo dataSetInfo;
    dataSetInfo.id = "performance-guard-data";
    dataSetInfo.displayName = "Performance guard data";
    dataSetInfo.version = "1";
    dataSetInfo.provenance = "synthetic performance guard";
    dataSetInfo.dateRanges.push_back(
        skygate::ephemeris::EphemerisDateRange{
            .id = "guard-range",
            .displayName = "Guard range",
            .start =
                {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
            .end = {.julianDatePart1 = 2'700'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        }
    );
    return dataSetInfo;
}

}  // namespace skygate::ui::tests
