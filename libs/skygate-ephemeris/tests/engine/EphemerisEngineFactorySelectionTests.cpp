#include "EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryBody()
{
    return {
        .id = "factory-target",
        .displayName = "Factory Target",
        .type = skygate::ephemeris::CelestialBodyType::Star,
        .fixedEquatorial =
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 11.25,
                .declinationDeg = -6.5,
            },
    };
}

[[nodiscard]] skygate::core::SkyContext makeContext()
{
    skygate::core::SkyContext context;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

}  // namespace

class EphemerisEngineFactorySelectionTests final : public QObject {
    Q_OBJECT

private slots:
    void createsRequestedSimpleEngineWithCatalogAndOptions();
    void fallsBackToSimpleWhenHighPrecisionIsUnavailableAndFallbackIsAllowed();
    void failsDefaultHighPrecisionRequestWhenHighPrecisionIsUnavailable();
    void failsStrictHighPrecisionRequestWhenHighPrecisionIsUnavailable();
};

void EphemerisEngineFactorySelectionTests::createsRequestedSimpleEngineWithCatalogAndOptions()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    request.options.enableAtmosphericRefraction = true;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::LightTime)
    );
    QVERIFY(options.enableAtmosphericRefraction);

    const auto state = result.engine->computeBodyState(makeContext(), "factory-target");
    QVERIFY(state.has_value());
    QCOMPARE(state->bodyIndex, 0U);
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QCOMPARE(state->equatorial.declinationDeg, -6.5);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
}

void EphemerisEngineFactorySelectionTests::fallsBackToSimpleWhenHighPrecisionIsUnavailableAndFallbackIsAllowed()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent;
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.usedSimpleEngineFallback());
    QVERIFY(result.hasDiagnostics());
    QVERIFY(!result.hasErrors());
    QVERIFY(result.diagnostics.size() >= std::size_t{1});
    QVERIFY(!result.diagnostics.front().displayText().empty());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::Apparent)
    );

    const auto state = result.engine->computeBodyState(makeContext(), std::size_t{0});
    QVERIFY(state.has_value());
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QVERIFY(state->metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
}

void EphemerisEngineFactorySelectionTests::failsDefaultHighPrecisionRequestWhenHighPrecisionIsUnavailable()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;

    const auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
        )
    );
    QVERIFY(result.hasDiagnostics());
    QVERIFY(result.hasErrors());
    QVERIFY(result.diagnostics.size() >= std::size_t{1});
    QVERIFY(result.diagnostics.front().isError());
    QVERIFY(!result.diagnostics.front().displayText().empty());
}

void EphemerisEngineFactorySelectionTests::failsStrictHighPrecisionRequestWhenHighPrecisionIsUnavailable()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    const auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
        )
    );
    QVERIFY(result.hasDiagnostics());
    QVERIFY(result.hasErrors());
    QVERIFY(result.diagnostics.size() >= std::size_t{1});
    QVERIFY(result.diagnostics.front().isError());
    QVERIFY(!result.diagnostics.front().displayText().empty());
}

QTEST_APPLESS_MAIN(EphemerisEngineFactorySelectionTests)

#include "EphemerisEngineFactorySelectionTests.moc"
