#pragma once

#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyLogging.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SettingsTestFixture.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include "skygate/core/ITimeSource.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSignalSpy>
#include <QTimeZone>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#endif

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

QDateTime fixedNowUtc()
{
    return QDateTime(QDate(2026, 5, 6), QTime(9, 30, 0), QTimeZone::UTC);
}

class FakeTimeSource final : public skygate::core::ITimeSource {
public:
    explicit FakeTimeSource(const QDateTime& nowUtc = fixedNowUtc())
        : m_nowUtc(nowUtc.toUTC())
    {
    }

    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(m_nowUtc);
    }

    void setNowUtc(const QDateTime& nowUtc)
    {
        m_nowUtc = nowUtc.toUTC();
    }

    void advanceSeconds(const int seconds)
    {
        m_nowUtc = m_nowUtc.addSecs(seconds);
    }

private:
    QDateTime m_nowUtc;
};

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> createTestCatalog()
{
    return skygate::ephemeris::createBundledStarCatalog();
}

std::unique_ptr<skygate::ephemeris::IEphemerisEngine> createTestEphemerisEngine(
    const skygate::ephemeris::IStarCatalog& starCatalog
)
{
    return skygate::ephemeris::createEphemerisEngine(starCatalog);
}

SkyContextController::InitializationOptions controllerInitializationOptions(
    bool loadSettings,
    const skygate::core::ITimeSource* timeSource = nullptr
)
{
    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = loadSettings;
    initializationOptions.initializeLocation = false;
    initializationOptions.timeSource = timeSource;
    return initializationOptions;
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    bool loadSettings = false
);

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    bool loadSettings,
    const skygate::core::ITimeSource* timeSource
);

std::unique_ptr<SkyContextController> createControllerWithOptions(
    SkyContextController::InitializationOptions initializationOptions
);

std::unique_ptr<SkyContextController> createController(bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings);
}

std::unique_ptr<SkyContextController> createControllerWithTimeSource(
    const skygate::core::ITimeSource& timeSource,
    const bool loadSettings = false
)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        loadSettings,
        &timeSource
    );
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings,
    const skygate::core::ITimeSource* timeSource
)
{
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        controllerInitializationOptions(loadSettings, timeSource),
        nullptr
    );
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings
)
{
    return createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        loadSettings,
        nullptr
    );
}

std::unique_ptr<SkyContextController> createControllerWithOptions(
    SkyContextController::InitializationOptions initializationOptions
)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        initializationOptions,
        nullptr
    );
}

QDateTime controllerUtcTime(const SkyContextController& controller)
{
    return skygate::ui::internal::SkyContextTimeCodec::toQDateTimeUtc(controller.skyContext().utcTime);
}

const skygate::ephemeris::CelestialBodyState* findStateById(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const std::string& bodyId
)
{
    for (const auto& state : snapshot.states) {
        if (snapshot.bodyAt(state.bodyIndex).id == bodyId) {
            return &state;
        }
    }

    return nullptr;
}

void configureFocusTestContext(SkyContextController& controller)
{
    controller.setLive(false);
    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");
}

std::unique_ptr<SkyContextController> createSingleBodyController(
    const std::string& id = "demo_target",
    const std::string& displayName = "Demo Target",
    const skygate::core::ITimeSource* timeSource = nullptr
)
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            id,
            displayName,
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
    });
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    auto controller = createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        false,
        timeSource
    );
    configureFocusTestContext(*controller);
    return controller;
}

double azimuthDifferenceDeg(const double lhs, const double rhs)
{
    const double difference = std::abs(lhs - rhs);
    return std::min(difference, 360.0 - difference);
}

#if SKYGATE_HAS_POSITIONING
class FakePositionSource final : public QGeoPositionInfoSource {
public:
    explicit FakePositionSource(QObject* parent = nullptr)
        : QGeoPositionInfoSource(parent)
    {
    }

    [[nodiscard]] PositioningMethods supportedPositioningMethods() const override
    {
        return SatellitePositioningMethods;
    }

    [[nodiscard]] int minimumUpdateInterval() const override
    {
        return 0;
    }

    [[nodiscard]] Error error() const override
    {
        return m_error;
    }

    [[nodiscard]] QGeoPositionInfo lastKnownPosition(
        bool fromSatellitePositioningMethodsOnly = false
    ) const override
    {
        (void)fromSatellitePositioningMethodsOnly;
        return m_lastPosition;
    }

    void startUpdates() override {}
    void stopUpdates() override {}

    void requestUpdate(const int timeoutMs = 0) override
    {
        m_lastTimeoutMs = timeoutMs;
        ++m_requestCount;
    }

    [[nodiscard]] int requestCount() const noexcept
    {
        return m_requestCount;
    }

    [[nodiscard]] int lastTimeoutMs() const noexcept
    {
        return m_lastTimeoutMs;
    }

    void publishPosition(
        const double latitudeDeg,
        const double longitudeDeg,
        const double altitudeMeters
    )
    {
        QGeoCoordinate coordinate(latitudeDeg, longitudeDeg, altitudeMeters);
        QGeoPositionInfo positionInfo(coordinate, fixedNowUtc());
        m_lastPosition = positionInfo;
        emit positionUpdated(positionInfo);
    }

    void publishInvalidPosition()
    {
        emit positionUpdated(QGeoPositionInfo());
    }

    void publishError(const Error error)
    {
        m_error = error;
        emit errorOccurred(error);
    }

private:
    Error m_error = NoError;
    QGeoPositionInfo m_lastPosition;
    int m_requestCount = 0;
    int m_lastTimeoutMs = 0;
};
#endif

}  // namespace
