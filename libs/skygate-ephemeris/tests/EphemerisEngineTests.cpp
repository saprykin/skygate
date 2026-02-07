#include "TestSupport.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <string_view>

namespace skygate::ephemeris::tests {
namespace {

const CelestialBodyState* findStateById(
    const SkySnapshot& snapshot,
    const std::string_view id
)
{
    for (const auto& state : snapshot.states) {
        if (state.body.id == id) {
            return &state;
        }
    }

    return nullptr;
}

bool runEphemerisEngineBaselineTests()
{
    bool success = true;

    std::unique_ptr<IStarCatalog> catalog = createBundledStarCatalog();
    success = expectTrue(catalog != nullptr, "Catalog should be created for engine test") && success;
    if (catalog == nullptr) {
        return false;
    }

    std::unique_ptr<IEphemerisEngine> engine = createEphemerisEngineStub(catalog.get());
    success = expectTrue(engine != nullptr, "Engine should be created") && success;
    if (engine == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.observer.elevationMeters = 16.0;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto snapshot = engine->compute(context);
    success = expectTrue(snapshot.states.size() == bodies.size(), "Engine output body count should match catalog") && success;
    success = expectTrue(
        snapshot.context.observer.latitudeDeg == context.observer.latitudeDeg,
        "Engine should preserve input context latitude"
    ) && success;

    const auto* sunState = findStateById(snapshot, "sun");
    const auto* moonState = findStateById(snapshot, "moon");
    const auto* siriusState = findStateById(snapshot, "sirius");

    success = expectTrue(sunState != nullptr, "Engine snapshot should contain sun state") && success;
    if (sunState != nullptr) {
        success = expectTrue(
            sunState->equatorial.rightAscensionHours > 0.0 &&
            sunState->equatorial.rightAscensionHours < 24.0 &&
            std::abs(sunState->equatorial.declinationDeg) <= 90.0 &&
            std::abs(sunState->horizontal.altitudeDeg) <= 90.0 &&
            sunState->horizontal.azimuthDeg >= 0.0 &&
            sunState->horizontal.azimuthDeg < 360.0,
            "Engine should compute baseline Sun position"
        ) && success;
    }

    success = expectTrue(moonState != nullptr, "Engine snapshot should contain moon state") && success;
    if (moonState != nullptr) {
        success = expectTrue(
            moonState->equatorial.rightAscensionHours > 0.0 &&
            moonState->equatorial.rightAscensionHours < 24.0 &&
            std::abs(moonState->equatorial.declinationDeg) <= 90.0 &&
            std::abs(moonState->horizontal.altitudeDeg) <= 90.0 &&
            moonState->horizontal.azimuthDeg >= 0.0 &&
            moonState->horizontal.azimuthDeg < 360.0,
            "Engine should compute baseline Moon position"
        ) && success;
    }

    success = expectTrue(siriusState != nullptr, "Engine snapshot should contain sirius state") && success;
    if (siriusState != nullptr) {
        success = expectNear(
            siriusState->equatorial.rightAscensionHours,
            6.7525,
            1e-4,
            "Sirius RA should match bundled fixed-star baseline"
        ) && success;
        success = expectNear(
            siriusState->equatorial.declinationDeg,
            -16.7161,
            1e-4,
            "Sirius Dec should match bundled fixed-star baseline"
        ) && success;
    }

    core::SkyContext nextDayContext = context;
    nextDayContext.utcTime += std::chrono::seconds(86400);
    const auto nextDaySnapshot = engine->compute(nextDayContext);

    const auto* nextDaySunState = findStateById(nextDaySnapshot, "sun");
    const auto* nextDayMoonState = findStateById(nextDaySnapshot, "moon");
    const auto* nextDaySiriusState = findStateById(nextDaySnapshot, "sirius");

    success = expectTrue(nextDaySunState != nullptr, "Next-day snapshot should contain sun state") && success;
    if (sunState != nullptr && nextDaySunState != nullptr) {
        success = expectTrue(
            std::abs(sunState->equatorial.rightAscensionHours - nextDaySunState->equatorial.rightAscensionHours) > 1e-5,
            "Sun RA should change over a day"
        ) && success;
    }

    success = expectTrue(nextDayMoonState != nullptr, "Next-day snapshot should contain moon state") && success;
    if (moonState != nullptr && nextDayMoonState != nullptr) {
        success = expectTrue(
            std::abs(moonState->equatorial.rightAscensionHours - nextDayMoonState->equatorial.rightAscensionHours) > 1e-4,
            "Moon RA should change over a day"
        ) && success;
    }

    success = expectTrue(nextDaySiriusState != nullptr, "Next-day snapshot should contain sirius state") && success;
    if (siriusState != nullptr && nextDaySiriusState != nullptr) {
        success = expectNear(
            siriusState->equatorial.rightAscensionHours,
            nextDaySiriusState->equatorial.rightAscensionHours,
            1e-8,
            "Fixed star RA should remain stable across time"
        ) && success;
        success = expectNear(
            siriusState->equatorial.declinationDeg,
            nextDaySiriusState->equatorial.declinationDeg,
            1e-8,
            "Fixed star Dec should remain stable across time"
        ) && success;
    }

    std::unique_ptr<IEphemerisEngine> nullCatalogEngine = createEphemerisEngineStub(nullptr);
    success = expectTrue(nullCatalogEngine != nullptr, "Engine should still be constructible with null catalog") && success;
    if (nullCatalogEngine != nullptr) {
        const auto nullSnapshot = nullCatalogEngine->compute(context);
        success = expectTrue(nullSnapshot.states.empty(), "Engine with null catalog should emit no states") && success;
    }

    std::unique_ptr<IStarCatalog> importedCatalog = createStarCatalogFromRows(
        "demo_star|Demo Star|Star|4.0|12.5|-30.0\n"
    );
    std::unique_ptr<IEphemerisEngine> importedEngine = createEphemerisEngineStub(importedCatalog.get());
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

bool runEphemerisFallbackTests()
{
    bool success = true;

    std::unique_ptr<IStarCatalog> customCatalog = createStarCatalogFromRows(
        "sun|SunById|Star|0.0\n"
        "moon|MoonById|Planet|0.0\n"
        "fixed_sun|Fixed Sun|Sun|0.0|1.5|2.5\n"
        "unknown_star|Unknown Star|Star|0.0\n"
        "planet_x|Planet X|Planet|0.0\n"
        "orion|Orion|Constellation|1.0\n"
        "unknown_constellation|Unknown Constellation|Constellation|1.0\n"
    );
    success = expectTrue(customCatalog != nullptr, "Custom catalog should be created") && success;
    if (customCatalog == nullptr) {
        return false;
    }

    std::unique_ptr<IEphemerisEngine> customEngine = createEphemerisEngineStub(customCatalog.get());
    success = expectTrue(customEngine != nullptr, "Custom engine should be created") && success;
    if (customEngine == nullptr) {
        return false;
    }

    core::SkyContext context;
    context.observer.latitudeDeg = 10.0;
    context.observer.longitudeDeg = 20.0;
    context.observer.elevationMeters = 5.0;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1710000000));

    const auto snapshot = customEngine->compute(context);
    success = expectTrue(snapshot.states.size() == 7, "Custom catalog body count should be preserved") && success;
    success = expectTrue(
        snapshot.context.utcTime.time_since_epoch() == context.utcTime.time_since_epoch(),
        "Engine should preserve input context time"
    ) && success;

    const auto* sunById = findStateById(snapshot, "sun");
    const auto* moonById = findStateById(snapshot, "moon");
    const auto* fixedSun = findStateById(snapshot, "fixed_sun");
    const auto* unknownStar = findStateById(snapshot, "unknown_star");
    const auto* planetX = findStateById(snapshot, "planet_x");
    const auto* orion = findStateById(snapshot, "orion");
    const auto* unknownConstellation = findStateById(snapshot, "unknown_constellation");

    success = expectTrue(sunById != nullptr, "Sun-by-id state should be present") && success;
    if (sunById != nullptr) {
        success = expectFinite(sunById->equatorial.rightAscensionHours, "Sun-by-id RA should be finite") && success;
        success = expectFinite(sunById->equatorial.declinationDeg, "Sun-by-id Dec should be finite") && success;
    }

    success = expectTrue(moonById != nullptr, "Moon-by-id state should be present") && success;
    if (moonById != nullptr) {
        success = expectFinite(moonById->equatorial.rightAscensionHours, "Moon-by-id RA should be finite") && success;
        success = expectFinite(moonById->equatorial.declinationDeg, "Moon-by-id Dec should be finite") && success;
    }

    success = expectTrue(fixedSun != nullptr, "Fixed-sun state should be present") && success;
    if (fixedSun != nullptr) {
        success = expectNear(
            fixedSun->equatorial.rightAscensionHours,
            1.5,
            1e-8,
            "Fixed coordinates should take precedence over type-based sun computation for RA"
        ) && success;
        success = expectNear(
            fixedSun->equatorial.declinationDeg,
            2.5,
            1e-8,
            "Fixed coordinates should take precedence over type-based sun computation for Dec"
        ) && success;
    }

    success = expectTrue(unknownStar != nullptr, "Unknown star state should be present") && success;
    if (unknownStar != nullptr) {
        success = expectNan(
            unknownStar->equatorial.rightAscensionHours,
            "Unknown stars without fixed coordinates should produce NaN RA"
        ) && success;
        success = expectNan(
            unknownStar->equatorial.declinationDeg,
            "Unknown stars without fixed coordinates should produce NaN Dec"
        ) && success;
    }

    success = expectTrue(planetX != nullptr, "Unknown planet state should be present") && success;
    if (planetX != nullptr) {
        success = expectNan(
            planetX->equatorial.rightAscensionHours,
            "Planets without explicit computation support should produce NaN RA"
        ) && success;
        success = expectNan(
            planetX->equatorial.declinationDeg,
            "Planets without explicit computation support should produce NaN Dec"
        ) && success;
    }

    success = expectTrue(orion != nullptr, "Known constellation state should be present") && success;
    if (orion != nullptr) {
        success = expectNear(
            orion->equatorial.rightAscensionHours,
            5.5833,
            1e-4,
            "Known constellation should use built-in RA"
        ) && success;
        success = expectNear(
            orion->equatorial.declinationDeg,
            5.0,
            1e-4,
            "Known constellation should use built-in Dec"
        ) && success;
    }

    success = expectTrue(unknownConstellation != nullptr, "Unknown constellation state should be present") && success;
    if (unknownConstellation != nullptr) {
        success = expectNan(
            unknownConstellation->equatorial.rightAscensionHours,
            "Unknown constellations should produce NaN RA"
        ) && success;
    }

    core::SkyContext invalidObserverContext = context;
    invalidObserverContext.observer.latitudeDeg = 120.0;
    const auto invalidObserverSnapshot = customEngine->compute(invalidObserverContext);
    const auto* invalidObserverSun = findStateById(invalidObserverSnapshot, "sun");
    success = expectTrue(invalidObserverSun != nullptr, "Invalid observer snapshot should include sun state") && success;
    if (invalidObserverSun != nullptr) {
        success = expectFinite(
            invalidObserverSun->equatorial.rightAscensionHours,
            "Invalid observer should not prevent equatorial computation"
        ) && success;
        success = expectNan(
            invalidObserverSun->horizontal.altitudeDeg,
            "Invalid observer should skip horizontal altitude computation"
        ) && success;
        success = expectNan(
            invalidObserverSun->horizontal.azimuthDeg,
            "Invalid observer should skip horizontal azimuth computation"
        ) && success;
    }

    return success;
}

}  // namespace

bool runEphemerisEngineTests()
{
    bool success = true;
    success = runEphemerisEngineBaselineTests() && success;
    success = runEphemerisFallbackTests() && success;
    return success;
}

}  // namespace skygate::ephemeris::tests
