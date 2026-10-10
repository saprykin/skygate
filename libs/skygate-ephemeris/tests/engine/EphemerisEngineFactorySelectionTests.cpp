#include "factory/EphemerisEngineFactory.hpp"
#include "factory/IEphemerisDiagnosticsSink.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "factory-target";
    body.displayName = "Factory Target";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 11.25,
        .declinationDeg = -6.5,
    };
    return body;
}

template <typename BodyRange>
[[nodiscard]] std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog> makeCatalogHandle(const BodyRange& bodies)
{
    return std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
}

[[nodiscard]] skygate::core::ObservationContext makeContext()
{
    skygate::core::ObservationContext context;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryTestBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "vega";
    body.displayName = "Vega";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 18.6156,
        .declinationDeg = 38.7837,
    };
    return body;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryBehaviorBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "factory-behavior-target";
    body.displayName = "Factory Behavior Target";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 5.25,
        .declinationDeg = -12.75,
    };
    return body;
}

class TestStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit TestStarCatalog(std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies)
        : m_catalog(std::move(bodies))
    {
    }

    [[nodiscard]] const skygate::ephemeris::CelestialBodyCatalog& catalog() const noexcept override
    {
        return m_catalog;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies() const override
    {
        return m_catalog.bodies();
    }

private:
    skygate::ephemeris::CelestialBodyCatalog m_catalog;
};

class RecordingEphemerisDiagnosticsSink final : public skygate::ephemeris::IEphemerisDiagnosticsSink {
public:
    void
    recordFactoryCreationDiagnostic(const skygate::ephemeris::EphemerisFactoryCreationDiagnostic& diagnostic) override
    {
        m_diagnostics.push_back(diagnostic);
    }

    [[nodiscard]] const std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic>&
    diagnostics() const noexcept
    {
        return m_diagnostics;
    }

private:
    std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic> m_diagnostics;
};

}  // namespace

class EphemerisEngineFactorySelectionTests final : public QObject {
    Q_OBJECT

private slots:
    void createsRequestedSimpleEngineWithCatalogAndOptions();
    void fallsBackToSimpleWhenHighPrecisionIsUnavailableAndFallbackIsAllowed();
    void fallsBackDefaultHighPrecisionRequestWhenHighPrecisionIsUnavailable();
    void failsStrictHighPrecisionRequestWhenHighPrecisionIsUnavailable();
    void fallsBackWithAllowPolicyWhenRuntimeFallbackIsDisabled();
    void defaultFactoryRequestCreatesSaneSimpleEngine();
    void simpleRequestCreatesRequestedEngineWithOptions();
    void compatibilityOverloadsCreateSimpleEngines();
    void preservesSimpleFactoryCompatibilityOverloads();
    void invalidEngineKindReturnsStructuredInvalidRequest();
    void highPrecisionRequestFallsBackOnlyWhenAllowed();
    void exposesFactoryFallbackPolicyHelpers();
};

void EphemerisEngineFactorySelectionTests::createsRequestedSimpleEngineWithCatalogAndOptions()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
    request.options.setEnableAtmosphericRefraction(true);

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(result.isSuccess());
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::lightTime())
    );
    QVERIFY(options.enableAtmosphericRefraction());

    const auto state = result.engine->computeBodyState(makeContext(), "factory-target");
    QVERIFY(state.has_value());
    QCOMPARE(state->bodyIndex, 0U);
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QCOMPARE(state->equatorial.declinationDeg, -6.5);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void EphemerisEngineFactorySelectionTests::fallsBackToSimpleWhenHighPrecisionIsUnavailableAndFallbackIsAllowed()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    request.options.setFallbackToSimpleEngine(true);

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.usedSimpleEngineFallback());
    QVERIFY(result.hasDiagnostics());
    QVERIFY(!result.hasErrors());
    QVERIFY(result.diagnostics.size() >= std::size_t{1});
    QVERIFY(!result.diagnostics.front().displayText().empty());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparent())
    );

    const auto state = result.engine->computeBodyState(makeContext(), std::size_t{0});
    QVERIFY(state.has_value());
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QVERIFY(state->metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void EphemerisEngineFactorySelectionTests::fallsBackDefaultHighPrecisionRequestWhenHighPrecisionIsUnavailable()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.usedSimpleEngineFallback());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback)
    );
    QVERIFY(result.hasDiagnostics());
    QVERIFY(!result.hasErrors());
    QVERIFY(result.diagnostics.size() >= std::size_t{1});
    QVERIFY(!result.diagnostics.front().isError());
    QVERIFY(!result.diagnostics.front().displayText().empty());
}

void EphemerisEngineFactorySelectionTests::failsStrictHighPrecisionRequestWhenHighPrecisionIsUnavailable()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setFallbackToSimpleEngine(true);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(result.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
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

void EphemerisEngineFactorySelectionTests::fallsBackWithAllowPolicyWhenRuntimeFallbackIsDisabled()
{
    const std::array bodies{makeFactoryBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setFallbackToSimpleEngine(false);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.usedSimpleEngineFallback());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(!result.engine->options().fallbackToSimpleEngine());
}

void EphemerisEngineFactorySelectionTests::defaultFactoryRequestCreatesSaneSimpleEngine()
{
    const skygate::ephemeris::EphemerisEngineFactoryRequest request;
    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    const skygate::ephemeris::EphemerisEngineOptions options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections())
    );
    QVERIFY(!options.enableAtmosphericRefraction());
}

void EphemerisEngineFactorySelectionTests::simpleRequestCreatesRequestedEngineWithOptions()
{
    const std::array bodies{makeFactoryBehaviorBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    request.options.setCorrectionFlags(
        skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        | skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration()
    );

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

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
        static_cast<std::uint8_t>(options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags())
    );
}

void EphemerisEngineFactorySelectionTests::compatibilityOverloadsCreateSimpleEngines()
{
    const auto context = makeContext();
    const std::array bodies{makeFactoryBehaviorBody()};

    auto emptyEngineResult = skygate::ephemeris::EphemerisEngineFactory::create();
    QVERIFY(emptyEngineResult.isSuccess());
    const auto& emptyEngine = emptyEngineResult.engine;
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    auto initializerEngineResult = skygate::ephemeris::EphemerisEngineFactory::create({makeFactoryBehaviorBody()});
    QVERIFY(initializerEngineResult.isSuccess());
    const auto& initializerEngine = initializerEngineResult.engine;
    const auto initializerState = initializerEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(initializerState.has_value());
    QCOMPARE(initializerState->bodyIndex, 0U);

    const skygate::ephemeris::CelestialBodyCatalog spanCatalog(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
    auto spanEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(spanCatalog);
    QVERIFY(spanEngineResult.isSuccess());
    const auto& spanEngine = spanEngineResult.engine;
    const auto spanState = spanEngine->computeBodyState(context, std::size_t{0});
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->equatorial.rightAscensionHours, 5.25);
    QCOMPARE(spanState->equatorial.declinationDeg, -12.75);

    const TestStarCatalog catalog({makeFactoryBehaviorBody()});
    auto catalogEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(catalog);
    QVERIFY(catalogEngineResult.isSuccess());
    const auto& catalogEngine = catalogEngineResult.engine;
    const auto catalogState = catalogEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);
}

void EphemerisEngineFactorySelectionTests::preservesSimpleFactoryCompatibilityOverloads()
{
    skygate::core::ObservationContext context;
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    auto emptyEngineResult = skygate::ephemeris::EphemerisEngineFactory::create();
    QVERIFY(emptyEngineResult.isSuccess());
    QVERIFY(emptyEngineResult.engine != nullptr);
    const auto& emptyEngine = emptyEngineResult.engine;
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    auto emptyBraceEngineResult = skygate::ephemeris::EphemerisEngineFactory::create({});
    QVERIFY(emptyBraceEngineResult.isSuccess());
    QVERIFY(emptyBraceEngineResult.engine != nullptr);
    const auto& emptyBraceEngine = emptyBraceEngineResult.engine;
    QCOMPARE(
        static_cast<std::uint8_t>(emptyBraceEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(emptyBraceEngine->compute(context).states.empty());

    const std::array bodies{makeFactoryTestBody()};
    const skygate::ephemeris::CelestialBodyCatalog spanCatalog(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
    auto spanEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(spanCatalog);
    QVERIFY(spanEngineResult.isSuccess());
    const auto& spanEngine = spanEngineResult.engine;

    const auto spanState = spanEngine->computeBodyState(context, "vega");
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->bodyIndex, 0U);
    QCOMPARE(spanState->equatorial.rightAscensionHours, 18.6156);
    QCOMPARE(spanState->equatorial.declinationDeg, 38.7837);

    const TestStarCatalog catalog({makeFactoryTestBody()});
    auto catalogEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(catalog);
    QVERIFY(catalogEngineResult.isSuccess());
    const auto& catalogEngine = catalogEngineResult.engine;

    const auto catalogState = catalogEngine->computeBodyState(context, std::size_t{0});
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->bodyIndex, 0U);
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
    const auto requestResult = skygate::ephemeris::EphemerisEngineFactory::create(request);
    QVERIFY(requestResult.isSuccess());
    QVERIFY(requestResult.engine != nullptr);
    QVERIFY(!requestResult.usedSimpleEngineFallback());
    QVERIFY(!requestResult.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(requestResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
}

void EphemerisEngineFactorySelectionTests::invalidEngineKindReturnsStructuredInvalidRequest()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = static_cast<skygate::ephemeris::EphemerisEngineKind::Type>(std::uint8_t{255});

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

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

void EphemerisEngineFactorySelectionTests::highPrecisionRequestFallsBackOnlyWhenAllowed()
{
    const std::array bodies{makeFactoryBehaviorBody()};
    RecordingEphemerisDiagnosticsSink fallbackDiagnosticsSink;

    skygate::ephemeris::EphemerisEngineFactoryRequest fallbackRequest;
    fallbackRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    fallbackRequest.catalog = makeCatalogHandle(bodies);
    fallbackRequest.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    fallbackRequest.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    fallbackRequest.options.setFallbackToSimpleEngine(true);
    fallbackRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
    fallbackRequest.diagnosticsSink = &fallbackDiagnosticsSink;

    auto fallback = skygate::ephemeris::EphemerisEngineFactory::create(fallbackRequest);

    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QVERIFY(fallback.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback)
    );
    QCOMPARE(fallbackDiagnosticsSink.diagnostics().size(), fallback.diagnostics.size());
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackDiagnosticsSink.diagnostics().front().severity),
        static_cast<std::uint8_t>(fallback.diagnostics.front().severity)
    );

    RecordingEphemerisDiagnosticsSink strictDiagnosticsSink;
    skygate::ephemeris::EphemerisEngineFactoryRequest strictRequest = fallbackRequest;
    strictRequest.options.setFallbackToSimpleEngine(false);
    strictRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    strictRequest.diagnosticsSink = &strictDiagnosticsSink;

    const auto strict = skygate::ephemeris::EphemerisEngineFactory::create(strictRequest);

    QVERIFY(!strict.isSuccess());
    QVERIFY(strict.isFailure());
    QVERIFY(strict.engine == nullptr);
    QVERIFY(!strict.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(strict.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(strict.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QVERIFY(strict.hasDiagnostics());
    QVERIFY(strict.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(strict.status),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
        )
    );
    QCOMPARE(strictDiagnosticsSink.diagnostics().size(), strict.diagnostics.size());
    QCOMPARE(
        static_cast<std::uint8_t>(strictDiagnosticsSink.diagnostics().front().severity),
        static_cast<std::uint8_t>(strict.diagnostics.front().severity)
    );
    QVERIFY(strictDiagnosticsSink.diagnostics().front().isError());
}

void EphemerisEngineFactorySelectionTests::exposesFactoryFallbackPolicyHelpers()
{
    constexpr auto strictPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    constexpr auto fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(strictPolicy));
    QVERIFY(skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(fallbackPolicy));
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::displayName(strictPolicy)
        == std::string_view{"strict high precision"}
    );
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::displayName(fallbackPolicy)
        == std::string_view{"allow simple engine fallback"}
    );
}

QTEST_APPLESS_MAIN(EphemerisEngineFactorySelectionTests)

#include "EphemerisEngineFactorySelectionTests.moc"
