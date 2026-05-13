#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryBehaviorBody()
{
    return {
        .id = "factory-behavior-target",
        .displayName = "Factory Behavior Target",
        .type = skygate::ephemeris::CelestialBodyType::Star,
        .fixedEquatorial =
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 5.25,
                .declinationDeg = -12.75,
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

template <typename Opaque>
[[nodiscard]] std::shared_ptr<const Opaque> makeOpaqueHandle(const std::shared_ptr<int>& owner) noexcept
{
    return {owner, reinterpret_cast<const Opaque*>(owner.get())};
}

class TestStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit TestStarCatalog(std::vector<skygate::ephemeris::CelestialBody> bodies) : m_bodies(std::move(bodies)) {}

    [[nodiscard]] std::span<const skygate::ephemeris::CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<skygate::ephemeris::CelestialBody> m_bodies;
};

}  // namespace

class EphemerisEngineFactoryBehaviorTests final : public QObject {
    Q_OBJECT

private slots:
    void requestCarriesCatalogOptionsAndOpaqueInputs();
    void resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics();
    void compatibilityOverloadsCreateSimpleEngines();
    void simpleRequestCreatesRequestedEngineWithOptions();
    void highPrecisionRequestFallsBackOnlyWhenAllowed();
    void invalidEngineKindReturnsStructuredInvalidRequest();
};

void EphemerisEngineFactoryBehaviorTests::requestCarriesCatalogOptionsAndOpaqueInputs()
{
    const std::array bodies{makeFactoryBehaviorBody()};
    const auto opaqueOwner = std::make_shared<int>(42);

    skygate::ephemeris::EphemerisDataSetInfo manifest;
    manifest.id = "test-manifest";
    manifest.displayName = "Test Manifest";

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    request.dataSetManifest = &manifest;
    request.activeDataSnapshot = makeOpaqueHandle<skygate::ephemeris::IEphemerisDataSnapshot>(opaqueOwner);
    request.timeScaleService = makeOpaqueHandle<skygate::ephemeris::ITimeScaleService>(opaqueOwner);
    request.earthOrientationProvider = makeOpaqueHandle<skygate::ephemeris::IEarthOrientationProvider>(opaqueOwner);
    request.diagnosticsSink = reinterpret_cast<skygate::ephemeris::IEphemerisDiagnosticsSink*>(opaqueOwner.get());
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(request.catalogBodies.size(), std::size_t{1});
    QVERIFY(request.catalogBodies.front().id == std::string{"factory-behavior-target"});
    QCOMPARE(
        static_cast<std::uint32_t>(request.options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric)
    );
    QVERIFY(request.dataSetManifest == &manifest);
    QVERIFY(request.activeDataSnapshot != nullptr);
    QVERIFY(request.timeScaleService != nullptr);
    QVERIFY(request.earthOrientationProvider != nullptr);
    QVERIFY(request.diagnosticsSink != nullptr);
    QVERIFY(skygate::ephemeris::allowsSimpleEngineFallback(request.fallbackPolicy));
}

void EphemerisEngineFactoryBehaviorTests::resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics()
{
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic warning{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic error{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "invalid engine kind",
    };

    auto success =
        skygate::ephemeris::EphemerisEngineFactoryResult::success(skygate::ephemeris::createEphemerisEngine());
    QVERIFY(success.isSuccess());
    QVERIFY(!success.isFailure());
    QVERIFY(!success.usedSimpleEngineFallback());
    QVERIFY(!success.hasDiagnostics());
    QVERIFY(!success.hasErrors());

    auto fallback = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        skygate::ephemeris::createEphemerisEngine(),
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {warning}
    );
    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QVERIFY(!fallback.diagnostics.front().displayText().empty());

    auto failure = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest, {error}
    );
    QVERIFY(!failure.isSuccess());
    QVERIFY(failure.isFailure());
    QVERIFY(!failure.usedSimpleEngineFallback());
    QVERIFY(failure.hasDiagnostics());
    QVERIFY(failure.hasErrors());
    QVERIFY(failure.diagnostics.front().displayText() == std::string_view{"invalid engine kind"});
}

void EphemerisEngineFactoryBehaviorTests::compatibilityOverloadsCreateSimpleEngines()
{
    const auto context = makeContext();
    const std::array bodies{makeFactoryBehaviorBody()};

    const auto emptyEngine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(emptyEngine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    const auto initializerEngine = skygate::ephemeris::createEphemerisEngine({makeFactoryBehaviorBody()});
    QVERIFY(initializerEngine != nullptr);
    const auto initializerState = initializerEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(initializerState.has_value());
    QCOMPARE(initializerState->bodyIndex, 0U);

    const auto spanEngine = skygate::ephemeris::createEphemerisEngine(
        std::span<const skygate::ephemeris::CelestialBody>{bodies.data(), bodies.size()}
    );
    QVERIFY(spanEngine != nullptr);
    const auto spanState = spanEngine->computeBodyState(context, std::size_t{0});
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->equatorial.rightAscensionHours, 5.25);
    QCOMPARE(spanState->equatorial.declinationDeg, -12.75);

    const TestStarCatalog catalog({makeFactoryBehaviorBody()});
    const auto catalogEngine = skygate::ephemeris::createEphemerisEngine(catalog);
    QVERIFY(catalogEngine != nullptr);
    const auto catalogState = catalogEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);
}

void EphemerisEngineFactoryBehaviorTests::simpleRequestCreatesRequestedEngineWithOptions()
{
    const std::array bodies{makeFactoryBehaviorBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                                      | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags), static_cast<std::uint32_t>(request.options.correctionFlags)
    );
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestFallsBackOnlyWhenAllowed()
{
    const std::array bodies{makeFactoryBehaviorBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest fallbackRequest;
    fallbackRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    fallbackRequest.catalogBodies = bodies;
    fallbackRequest.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    fallbackRequest.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent;
    fallbackRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    auto fallback = skygate::ephemeris::createEphemerisEngine(fallbackRequest);

    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QVERIFY(fallback.engine != nullptr);
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback)
    );

    skygate::ephemeris::EphemerisEngineFactoryRequest strictRequest = fallbackRequest;
    strictRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    const auto strict = skygate::ephemeris::createEphemerisEngine(strictRequest);

    QVERIFY(!strict.isSuccess());
    QVERIFY(strict.isFailure());
    QVERIFY(strict.engine == nullptr);
    QVERIFY(!strict.usedSimpleEngineFallback());
    QVERIFY(strict.hasDiagnostics());
    QVERIFY(strict.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(strict.status),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
        )
    );
}

void EphemerisEngineFactoryBehaviorTests::invalidEngineKindReturnsStructuredInvalidRequest()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = static_cast<skygate::ephemeris::EphemerisEngineKind>(std::uint8_t{255});

    const auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(result.hasDiagnostics());
    QVERIFY(result.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest)
    );
    QVERIFY(result.diagnostics.front().isError());
    QVERIFY(!result.diagnostics.front().displayText().empty());
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryBehaviorTests)

#include "EphemerisEngineFactoryBehaviorTests.moc"
