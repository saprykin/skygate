#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace {

bool expectTrue(const bool condition, const std::string& message)
{
    if (condition) {
        return true;
    }

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool expectNear(const double value, const double expected, const double tolerance, const std::string& message)
{
    return expectTrue(std::abs(value - expected) <= tolerance, message);
}

bool runCatalogLoaderTests()
{
    bool success = true;

    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog =
        skygate::ephemeris::createBundledStarCatalog();
    success = expectTrue(catalog != nullptr, "Bundled catalog should be created") && success;
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    success = expectTrue(!bodies.empty(), "Bundled catalog should include at least one body") && success;
    success = expectTrue(bodies.size() == 25, "Bundled catalog should include expected initial body set") && success;

    bool hasSun = false;
    bool hasSirius = false;
    bool hasOrionConstellation = false;
    for (const auto& body : bodies) {
        if (body.id == "sun" && body.type == skygate::ephemeris::CelestialBodyType::Sun) {
            hasSun = true;
        }

        if (body.id == "sirius" && body.type == skygate::ephemeris::CelestialBodyType::Star) {
            hasSirius = true;
        }

        if (body.id == "orion" && body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            hasOrionConstellation = true;
        }
    }

    success = expectTrue(hasSun, "Bundled catalog should contain Sun") && success;
    success = expectTrue(hasSirius, "Bundled catalog should contain Sirius") && success;
    success = expectTrue(hasOrionConstellation, "Bundled catalog should contain Orion constellation") && success;

    std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedLikeCatalog =
        skygate::ephemeris::createStarCatalogFromRows(
            "vega|Vega|Star|0.03|18.6156|38.7837\n"
            "orion|Orion|Constellation|1.6\n"
        );
    success = expectTrue(downloadedLikeCatalog != nullptr, "Downloaded-like catalog text should parse") && success;
    if (downloadedLikeCatalog != nullptr) {
        const auto parsedBodies = downloadedLikeCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 2, "Downloaded-like catalog should contain two rows") && success;
        success = expectTrue(
            parsedBodies[0].fixedEquatorial.has_value(),
            "Downloaded-like catalog should parse optional RA/Dec coordinates"
        ) && success;
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> hygCatalog =
        skygate::ephemeris::createStarCatalogFromHygCsv(
            "id,hip,proper,ra,dec,mag\n"
            "1,32349,Sirius,6.7525,-16.7161,-1.46\n"
            "2,24608,Capella,5.2782,45.9979,0.08\n"
        );
    success = expectTrue(hygCatalog != nullptr, "HYG CSV catalog should parse") && success;
    if (hygCatalog != nullptr) {
        const auto parsedBodies = hygCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 2, "HYG CSV parser should load expected rows") && success;
        success = expectTrue(
            parsedBodies[0].fixedEquatorial.has_value(),
            "HYG CSV parser should attach fixed equatorial coordinates"
        ) && success;
        success = expectTrue(
            parsedBodies[0].id == "hip_32349",
            "HYG CSV parser should prefer HIP-based IDs when available"
        ) && success;
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> hygWithoutHipCatalog =
        skygate::ephemeris::createStarCatalogFromHygCsv(
            "id,proper,ra,dec,mag\n"
            "77,NoHipStar,1.5,2.5,4.2\n"
        );
    success = expectTrue(hygWithoutHipCatalog != nullptr, "HYG CSV parser should support rows without HIP") && success;
    if (hygWithoutHipCatalog != nullptr) {
        const auto parsedBodies = hygWithoutHipCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 1, "HYG CSV parser should load row without HIP") && success;
        if (!parsedBodies.empty()) {
            success = expectTrue(
                parsedBodies[0].id == "hyg_77",
                "HYG CSV parser should fall back to HYG ID when HIP is missing"
            ) && success;
        }
    }

    return success;
}

bool runProjectionTests()
{
    bool success = true;

    std::unique_ptr<skygate::core::IProjection> projection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    success = expectTrue(projection != nullptr, "Stereographic projection should be created") && success;
    if (projection == nullptr) {
        return false;
    }

    skygate::core::ProjectionParams params;
    params.center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0};
    params.fovDeg = 90.0;
    params.rollDeg = 0.0;
    params.viewportWidth = 1100.0;
    params.viewportHeight = 760.0;

    const auto centeredPoint = projection->project(params.center, params);
    success = expectTrue(centeredPoint.isVisible, "Center direction should be visible") && success;
    success = expectNear(centeredPoint.x, params.viewportWidth * 0.5, 1e-6, "Center direction x should map to screen center") && success;
    success = expectNear(centeredPoint.y, params.viewportHeight * 0.5, 1e-6, "Center direction y should map to screen center") && success;

    const auto oppositePoint = projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = -45.0, .azimuthDeg = 0.0},
        params
    );
    success = expectTrue(!oppositePoint.isVisible, "Opposite direction should not be visible") && success;

    skygate::core::ProjectionParams invalidParams = params;
    invalidParams.fovDeg = 0.0;
    const auto invalidPoint = projection->project(params.center, invalidParams);
    success = expectTrue(!invalidPoint.isVisible, "Invalid FOV should return not visible") && success;

    return success;
}

bool runEphemerisEngineTests()
{
    bool success = true;

    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog =
        skygate::ephemeris::createBundledStarCatalog();
    success = expectTrue(catalog != nullptr, "Catalog should be created for engine test") && success;
    if (catalog == nullptr) {
        return false;
    }

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> engine =
        skygate::ephemeris::createEphemerisEngineStub(catalog.get());
    success = expectTrue(engine != nullptr, "Engine should be created") && success;
    if (engine == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.observer.elevationMeters = 16.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto snapshot = engine->compute(context);
    success = expectTrue(snapshot.states.size() == bodies.size(), "Engine output body count should match catalog") && success;
    success = expectTrue(snapshot.context.observer.latitudeDeg == context.observer.latitudeDeg, "Engine should preserve input context latitude") && success;

    bool hasSunWithPosition = false;
    bool hasMoonWithPosition = false;
    bool hasSiriusWithFixedPosition = false;

    for (const auto& state : snapshot.states) {
        if (state.body.id == "sun") {
            hasSunWithPosition = state.equatorial.rightAscensionHours > 0.0
                && state.equatorial.rightAscensionHours < 24.0
                && std::abs(state.equatorial.declinationDeg) <= 90.0
                && std::abs(state.horizontal.altitudeDeg) <= 90.0
                && state.horizontal.azimuthDeg >= 0.0
                && state.horizontal.azimuthDeg < 360.0;
        }

        if (state.body.id == "moon") {
            hasMoonWithPosition = state.equatorial.rightAscensionHours > 0.0
                && state.equatorial.rightAscensionHours < 24.0
                && std::abs(state.equatorial.declinationDeg) <= 90.0
                && std::abs(state.horizontal.altitudeDeg) <= 90.0
                && state.horizontal.azimuthDeg >= 0.0
                && state.horizontal.azimuthDeg < 360.0;
        }

        if (state.body.id == "sirius") {
            hasSiriusWithFixedPosition = expectNear(
                state.equatorial.rightAscensionHours,
                6.7525,
                1e-4,
                "Sirius RA should match bundled fixed-star baseline"
            ) && expectNear(
                state.equatorial.declinationDeg,
                -16.7161,
                1e-4,
                "Sirius Dec should match bundled fixed-star baseline"
            );
        }
    }

    success = expectTrue(hasSunWithPosition, "Engine should compute baseline Sun position") && success;
    success = expectTrue(hasMoonWithPosition, "Engine should compute baseline Moon position") && success;
    success = expectTrue(hasSiriusWithFixedPosition, "Engine should compute fixed-star baseline coordinates") && success;

    skygate::core::SkyContext nextDayContext = context;
    nextDayContext.utcTime += std::chrono::seconds(86400);
    const auto nextDaySnapshot = engine->compute(nextDayContext);

    for (const auto& state : snapshot.states) {
        for (const auto& nextDayState : nextDaySnapshot.states) {
            if (state.body.id != nextDayState.body.id) {
                continue;
            }

            if (state.body.id == "sirius") {
                success = expectNear(
                    state.equatorial.rightAscensionHours,
                    nextDayState.equatorial.rightAscensionHours,
                    1e-8,
                    "Fixed star RA should remain stable across time"
                ) && success;
                success = expectNear(
                    state.equatorial.declinationDeg,
                    nextDayState.equatorial.declinationDeg,
                    1e-8,
                    "Fixed star Dec should remain stable across time"
                ) && success;
            }

            if (state.body.id == "sun") {
                success = expectTrue(
                    std::abs(state.equatorial.rightAscensionHours - nextDayState.equatorial.rightAscensionHours) > 1e-5,
                    "Sun RA should change over a day"
                ) && success;
            }

            if (state.body.id == "moon") {
                success = expectTrue(
                    std::abs(state.equatorial.rightAscensionHours - nextDayState.equatorial.rightAscensionHours) > 1e-4,
                    "Moon RA should change over a day"
                ) && success;
            }
        }
    }

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> nullCatalogEngine =
        skygate::ephemeris::createEphemerisEngineStub(nullptr);
    success = expectTrue(nullCatalogEngine != nullptr, "Engine should still be constructible with null catalog") && success;
    if (nullCatalogEngine != nullptr) {
        const auto nullSnapshot = nullCatalogEngine->compute(context);
        success = expectTrue(nullSnapshot.states.empty(), "Engine with null catalog should emit no states") && success;
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> importedCatalog =
        skygate::ephemeris::createStarCatalogFromRows(
            "demo_star|Demo Star|Star|4.0|12.5|-30.0\n"
        );
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> importedEngine =
        skygate::ephemeris::createEphemerisEngineStub(importedCatalog.get());
    success = expectTrue(importedEngine != nullptr, "Engine should support imported catalog stars") && success;
    if (importedEngine != nullptr) {
        const auto importedSnapshot = importedEngine->compute(context);
        success = expectTrue(importedSnapshot.states.size() == 1, "Imported catalog should emit one state") && success;
        if (!importedSnapshot.states.empty()) {
            success = expectNear(
                importedSnapshot.states[0].equatorial.rightAscensionHours,
                12.5,
                1e-8,
                "Imported star RA should come from catalog fixed coordinates"
            ) && success;
            success = expectNear(
                importedSnapshot.states[0].equatorial.declinationDeg,
                -30.0,
                1e-8,
                "Imported star Dec should come from catalog fixed coordinates"
            ) && success;
        }
    }

    return success;
}

}  // namespace

int main()
{
    bool success = true;
    success = runCatalogLoaderTests() && success;
    success = runProjectionTests() && success;
    success = runEphemerisEngineTests() && success;

    if (!success) {
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
