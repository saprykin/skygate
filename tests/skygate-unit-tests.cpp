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
    success = expectTrue(bodies.size() == 15, "Bundled catalog should include expected initial body set") && success;

    bool hasSun = false;
    bool hasSirius = false;
    for (const auto& body : bodies) {
        if (body.id == "sun" && body.type == skygate::ephemeris::CelestialBodyType::Sun) {
            hasSun = true;
        }

        if (body.id == "sirius" && body.type == skygate::ephemeris::CelestialBodyType::Star) {
            hasSirius = true;
        }
    }

    success = expectTrue(hasSun, "Bundled catalog should contain Sun") && success;
    success = expectTrue(hasSirius, "Bundled catalog should contain Sirius") && success;
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
    context.observer.latitudeDeg = 1.2;
    context.observer.longitudeDeg = 3.4;
    context.observer.elevationMeters = 5.6;

    const auto snapshot = engine->compute(context);
    success = expectTrue(snapshot.states.size() == bodies.size(), "Engine output body count should match catalog") && success;
    success = expectTrue(snapshot.context.observer.latitudeDeg == context.observer.latitudeDeg, "Engine should preserve input context latitude") && success;

    if (!snapshot.states.empty() && !bodies.empty()) {
        success = expectTrue(snapshot.states.front().body.id == bodies.front().id, "Engine state body ordering should match catalog") && success;
    }

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> nullCatalogEngine =
        skygate::ephemeris::createEphemerisEngineStub(nullptr);
    success = expectTrue(nullCatalogEngine != nullptr, "Engine should still be constructible with null catalog") && success;
    if (nullCatalogEngine != nullptr) {
        const auto nullSnapshot = nullCatalogEngine->compute(context);
        success = expectTrue(nullSnapshot.states.empty(), "Engine with null catalog should emit no states") && success;
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
