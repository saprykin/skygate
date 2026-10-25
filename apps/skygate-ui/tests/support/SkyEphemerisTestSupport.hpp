#pragma once

#include "EquatorialCoordinate.hpp"
#include "ObservationContext.hpp"
#include "TestHelpers.hpp"
#include "app/SkyContextController.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/IStarCatalog.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/SkyTimeController.hpp"

#include <QDateTime>
#include <QString>
#include <QTimeZone>

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

// Fixed "now" shared by the UI fixtures so a pinned time source, a controller
// timeline, and the rendered scene all observe one deterministic instant.
[[nodiscard]] inline QDateTime fixedNowUtc()
{
    return QDateTime(QDate(2026, 5, 6), QTime(9, 30, 0), QTimeZone::UTC);
}

[[nodiscard]] inline skygate::ephemeris::OwnGalaxyCelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

[[nodiscard]] inline skygate::ephemeris::OwnGalaxyCelestialBody makeFixedBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
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
        skygate::core::EquatorialCoordinate{
            .rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg
        }
    );
}

[[nodiscard]] inline skygate::ephemeris::DistantCelestialBody makeDeepSkyBody(
    std::string id, std::string displayName, const double visualMagnitude, std::vector<std::string> aliases = {}
)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = std::move(aliases),
        .majorAxisArcmin = 8.0,
        .minorAxisArcmin = 4.0,
        .positionAngleDeg = 0.0,
    };
    return body;
}

[[nodiscard]] inline SkyContextController::InitializationOptions testControllerOptions(const bool loadSettings = false)
{
    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = loadSettings;
    initializationOptions.initializeLocation = false;
    return initializationOptions;
}

[[nodiscard]] inline std::unique_ptr<skygate::ephemeris::IStarCatalog>
createTestCatalog(std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies)
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(std::move(bodies));
}

[[nodiscard]] inline std::unique_ptr<skygate::ephemeris::IStarCatalog>
createTestCatalog(std::vector<skygate::ephemeris::DistantCelestialBody> bodies)
{
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(bodies.size());
    for (std::uint32_t index = 0U; index < bodies.size(); ++index) {
        order.push_back({.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = index});
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({}, std::move(bodies), std::move(order));
}

[[nodiscard]] inline std::unique_ptr<skygate::ephemeris::IEphemerisEngine>
createTestEphemerisEngine(const skygate::ephemeris::IStarCatalog& starCatalog)
{
    return std::move(skygate::ephemeris::EphemerisEngineFactory::create(starCatalog).engine);
}

[[nodiscard]] inline std::unique_ptr<SkyContextController> createTestController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings = false
)
{
    return std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), testControllerOptions(loadSettings), nullptr
    );
}

inline bool configureTestSkyContext(SkyContextController& controller, const TestSkyContextConfig& config = {})
{
    if (config.applyTimeZone && !controller.timeController()->setTimeZoneId(config.timeZoneId)) {
        return false;
    }

    controller.setLive(config.live);
    if (config.applyUtcDateTime) {
        controller.timeController()->setInitialTimeZoneId(QStringLiteral("UTC"));
        if (!controller.timeController()->setDateTimeText(config.utcDate, config.utcTime)) {
            return false;
        }
    }

    controller.setLatitudeText(config.latitude);
    controller.setLongitudeText(config.longitude);
    controller.setElevationText(config.elevation);
    return true;
}

[[nodiscard]] inline std::optional<skygate::ephemeris::CelestialBodyState>
findBodyStateById(const skygate::ephemeris::EphemerisSnapshot& snapshot, const std::string& bodyId)
{
    const skygate::ephemeris::CelestialBodyState* state = skygate::ephemeris::tests::findStateById(snapshot, bodyId);
    return state != nullptr ? std::optional<skygate::ephemeris::CelestialBodyState>(*state) : std::nullopt;
}

}  // namespace skygate::ui::tests
