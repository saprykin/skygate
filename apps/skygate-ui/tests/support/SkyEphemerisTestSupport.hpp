#pragma once

#include "SkyContextController.hpp"
#include "SkyTimeController.hpp"

#include "skygate/core/SkyTypes.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QString>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ui::tests {

struct TestSkyContextConfig {
    QString timeZoneId = QStringLiteral("UTC");
    QString utcDate = QStringLiteral("2024-06-01");
    QString utcTime = QStringLiteral("22:00:00");
    QString latitude = QStringLiteral("47.3769");
    QString longitude = QStringLiteral("8.5417");
    QString elevation = QStringLiteral("408.0");
    bool live = false;
    bool applyTimeZone = true;
    bool applyUtcDateTime = true;
};

[[nodiscard]] inline ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const std::optional<core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

[[nodiscard]] inline ephemeris::CelestialBody makeFixedBody(
    std::string id,
    std::string displayName,
    const ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    return makeBody(
        std::move(id),
        std::move(displayName),
        type,
        visualMagnitude,
        core::EquatorialCoordinate {
            .rightAscensionHours = rightAscensionHours,
            .declinationDeg = declinationDeg
        }
    );
}

[[nodiscard]] inline ephemeris::CelestialBody makeDeepSkyBody(
    std::string id,
    std::string displayName,
    const double visualMagnitude,
    std::vector<std::string> aliases = {}
)
{
    ephemeris::CelestialBody body = makeBody(
        std::move(id),
        std::move(displayName),
        ephemeris::CelestialBodyType::DeepSkyObject,
        visualMagnitude
    );
    body.deepSkyObject = ephemeris::DeepSkyObjectInfo {
        .kind = ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = std::move(aliases),
        .majorAxisArcmin = 8.0,
        .minorAxisArcmin = 4.0,
        .positionAngleDeg = 0.0,
    };
    return body;
}

[[nodiscard]] inline SkyContextController::InitializationOptions testControllerOptions(
    const bool loadSettings = false
)
{
    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = loadSettings;
    initializationOptions.initializeLocation = false;
    return initializationOptions;
}

[[nodiscard]] inline std::unique_ptr<ephemeris::IStarCatalog> createTestCatalog(
    std::vector<ephemeris::CelestialBody> bodies
)
{
    return ephemeris::createStarCatalogFromBodies(std::move(bodies));
}

[[nodiscard]] inline std::unique_ptr<ephemeris::IEphemerisEngine> createTestEphemerisEngine(
    const ephemeris::IStarCatalog& starCatalog
)
{
    return ephemeris::createEphemerisEngine(starCatalog);
}

[[nodiscard]] inline std::unique_ptr<SkyContextController> createTestController(
    std::unique_ptr<ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings = false
)
{
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        testControllerOptions(loadSettings),
        nullptr
    );
}

[[nodiscard]] inline std::unique_ptr<SkyContextController> createTestController(
    std::vector<ephemeris::CelestialBody> bodies,
    const bool loadSettings = false
)
{
    auto starCatalog = createTestCatalog(std::move(bodies));
    if (starCatalog == nullptr) {
        return nullptr;
    }

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    if (ephemerisEngine == nullptr) {
        return nullptr;
    }

    return createTestController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings);
}

inline bool configureTestSkyContext(
    SkyContextController& controller,
    const TestSkyContextConfig& config = {}
)
{
    if (config.applyTimeZone && !controller.timeController()->setTimeZoneId(config.timeZoneId)) {
        return false;
    }

    controller.setLive(config.live);
    if (config.applyUtcDateTime && !controller.setUtcDateTimeText(config.utcDate, config.utcTime)) {
        return false;
    }

    controller.setLatitudeText(config.latitude);
    controller.setLongitudeText(config.longitude);
    controller.setElevationText(config.elevation);
    return true;
}

[[nodiscard]] inline std::optional<ephemeris::CelestialBodyState> findBodyStateById(
    const ephemeris::SkySnapshot& snapshot,
    const std::string& bodyId
)
{
    for (const ephemeris::CelestialBodyState& state : snapshot.states) {
        if (snapshot.bodyAt(state.bodyIndex).id == bodyId) {
            return state;
        }
    }

    return std::nullopt;
}

}  // namespace skygate::ui::tests
