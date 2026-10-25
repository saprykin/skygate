#pragma once

#include "DeviceLocationTestSupport.hpp"
#include "ITimeSource.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "app/SkyContextController.hpp"
#include "time/SkyQtTimeCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "diagnostics/SkyLogging.hpp"
#include "theme/SkyOverlayLayerSettings.hpp"
#include "time/SkyTimeController.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSignalSpy>
#include <QTimeZone>
#include <QtTest>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

// Reuse the fixtures the shared support headers already provide instead of
// keeping local copies; these declarations keep the suites' unqualified calls
// working.
using skygate::ephemeris::tests::findStateById;
using skygate::ui::tests::createTestEphemerisEngine;
using skygate::ui::tests::fixedNowUtc;
using skygate::ui::tests::makeBody;
using skygate::ui::tests::testControllerOptions;

class FakeTimeSource final : public skygate::core::ITimeSource {
public:
    explicit FakeTimeSource(const QDateTime& nowUtc = fixedNowUtc()) : m_nowUtc(nowUtc.toUTC()) {}

    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return SkyQtTimeCodec::toUtcTimePoint(m_nowUtc);
    }

private:
    QDateTime m_nowUtc;
};

std::unique_ptr<skygate::ephemeris::IStarCatalog> createTestCatalog()
{
    return skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
}

SkyContextController::InitializationOptions
controllerInitializationOptions(bool loadSettings, const skygate::core::ITimeSource* timeSource = nullptr)
{
    SkyContextController::InitializationOptions initializationOptions = testControllerOptions(loadSettings);
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

std::unique_ptr<SkyContextController>
createControllerWithOptions(SkyContextController::InitializationOptions initializationOptions);

std::unique_ptr<SkyContextController> createController(bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings);
}

std::unique_ptr<SkyContextController>
createControllerWithTimeSource(const skygate::core::ITimeSource& timeSource, const bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings, &timeSource);
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
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings, nullptr);
}

std::unique_ptr<SkyContextController>
createControllerWithOptions(SkyContextController::InitializationOptions initializationOptions)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), initializationOptions, nullptr
    );
}

QDateTime controllerUtcTime(const SkyContextController& controller)
{
    return SkyQtTimeCodec::toQDateTimeUtc(controller.skyContext().utcTime);
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
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            id,
            displayName,
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            1.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.5, .declinationDeg = 2.5}
        ),
    });
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine), false, timeSource);
    configureFocusTestContext(*controller);
    return controller;
}

double azimuthDifferenceDeg(const double lhs, const double rhs)
{
    const double difference = std::abs(lhs - rhs);
    return std::min(difference, 360.0 - difference);
}

}  // namespace
