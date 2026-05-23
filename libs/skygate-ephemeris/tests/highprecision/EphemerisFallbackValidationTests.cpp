#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] CelestialBody makeSunBody()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = CelestialBodyType::Sun,
        .ephemerisSource = CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] CelestialBody makeUnsupportedBody()
{
    return {
        .id = "unsupported",
        .displayName = "Unsupported",
        .type = CelestialBodyType::DeepSkyObject,
        .ephemerisSource = CelestialBodyEphemerisSource::Unresolved,
    };
}

[[nodiscard]] core::SkyContext makeContext()
{
    core::SkyContext context;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;
    return request;
}

[[nodiscard]] HighPrecisionEphemerisEngineDependencies
makeDependencies(std::shared_ptr<ISolarSystemStateCalculator> solarSystemCalculator = {})
{
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = std::move(solarSystemCalculator);
    dependencies.dataSetInfo.id = "fallback-validation";
    dependencies.dataSetInfo.displayName = "Fallback validation";
    dependencies.dataSetInfo.version = "test";
    dependencies.dataSetInfo.provenance = "validation test";
    dependencies.dataSetInfo.dateRanges.push_back(
        EphemerisDateRange{
            .id = "modern",
            .displayName = "Modern",
            .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
            .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        }
    );
    return dependencies;
}

class StaticSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    explicit StaticSolarSystemCalculator(HighPrecisionCalculatorResult result) : m_result(std::move(result)) {}

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput&) const override
    {
        ++m_callCount;
        return m_result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    HighPrecisionCalculatorResult m_result;
    mutable int m_callCount = 0;
};

[[nodiscard]] HighPrecisionEphemerisEngine makeHighPrecisionEngine(HighPrecisionCalculatorResult calculatorResult)
{
    const std::array bodies{makeSunBody()};
    auto calculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    return HighPrecisionEphemerisEngine(bodies, makeRequest().options, makeDependencies(std::move(calculator)));
}

}  // namespace

class EphemerisFallbackValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void simpleFactoryCreationSucceeds();
    void highPrecisionUnavailableFallbackProducesWarnings();
    void strictHighPrecisionUnavailableProducesErrors();
    void missingLongRangeKernelFallbackIsDegraded();
    void staleDataWarningsRemainVisible();
    void unsupportedBodyReturnsStructuredWarning();
    void outOfRangeSolarSystemRequestUsesSimpleFallbackWhenEnabled();
    void outOfRangeRequestWithoutFallbackStaysOutOfRange();
    void failedRequestReturnsFailedStatus();
};

void EphemerisFallbackValidationTests::simpleFactoryCreationSucceeds()
{
    EphemerisEngineFactoryRequest request;
    request.engineKind = EphemerisEngineKind::Simple;
    request.options.engineKind = EphemerisEngineKind::Simple;

    const auto result = createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QCOMPARE(static_cast<std::uint8_t>(result.engine->kind()), static_cast<std::uint8_t>(EphemerisEngineKind::Simple));
}

void EphemerisFallbackValidationTests::highPrecisionUnavailableFallbackProducesWarnings()
{
    EphemerisEngineFactoryRequest request;
    request.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.fallbackPolicy = EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    const auto result = createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.usedSimpleEngineFallback());
    QVERIFY(result.engine != nullptr);
    QVERIFY(result.hasDiagnostics());
    QVERIFY(!result.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(EphemerisFactoryCreationStatus::CreatedSimpleFallback)
    );
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : result.diagnostics) {
        QCOMPARE(
            static_cast<std::uint8_t>(diagnostic.severity),
            static_cast<std::uint8_t>(EphemerisFactoryCreationDiagnosticSeverity::Warning)
        );
        QVERIFY(!diagnostic.displayText().empty());
    }
}

void EphemerisFallbackValidationTests::strictHighPrecisionUnavailableProducesErrors()
{
    EphemerisEngineFactoryRequest request;
    request.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.fallbackPolicy = EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    const auto result = createEphemerisEngine(request);

    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(result.hasDiagnostics());
    QVERIFY(result.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable)
    );
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : result.diagnostics) {
        QVERIFY(diagnostic.isError());
        QVERIFY(!diagnostic.displayText().empty());
    }
}

void EphemerisFallbackValidationTests::missingLongRangeKernelFallbackIsDegraded()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 7.5,
        .declinationDeg = -11.0,
    };
    calculatorResult.metadata.status = EphemerisResultStatus::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    calculatorResult.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
    calculatorResult.metadata.dataSourceProvenance = "missing DE441 long-range kernel; modern kernel fallback";
    calculatorResult.metadata.effectiveDataValidityRange = EphemerisDateRange{
        .id = "de440-modern",
        .displayName = "DE440 modern range",
        .start = {.julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'700'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(std::move(calculatorResult));
    const auto state = engine.computeBodyState(makeRequest(), "sun");

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QCOMPARE(
        state->metadata.dataSourceProvenance, std::string{"missing DE441 long-range kernel; modern kernel fallback"}
    );
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QCOMPARE(state->metadata.effectiveDataValidityRange->id, std::string{"de440-modern"});
    QCOMPARE(state->equatorial.rightAscensionHours, 7.5);
}

void EphemerisFallbackValidationTests::staleDataWarningsRemainVisible()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 8.0,
        .declinationDeg = 9.0,
    };
    calculatorResult.metadata.status = EphemerisResultStatus::Degraded;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    calculatorResult.metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    calculatorResult.metadata.dataSourceProvenance = "stale EOP and leap-second data";

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(std::move(calculatorResult));
    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QCOMPARE(state->metadata.warningCount(), std::size_t{2});
    QCOMPARE(state->metadata.dataSourceProvenance, std::string{"stale EOP and leap-second data"});
}

void EphemerisFallbackValidationTests::unsupportedBodyReturnsStructuredWarning()
{
    const std::array bodies{makeUnsupportedBody()};
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies());

    const auto state = engine.computeBodyState(makeRequest(), "unsupported");

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Unsupported)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::UnsupportedBody));
    QVERIFY(!state->metadata.dataSourceProvenance.empty());
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(state->equatorial.declinationDeg));
}

void EphemerisFallbackValidationTests::outOfRangeRequestWithoutFallbackStaysOutOfRange()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = EphemerisResultStatus::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "kernel out of range";

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(std::move(calculatorResult));
    EphemerisRequest request = makeRequest();
    request.options.fallbackToSimpleEngine = false;
    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::OutOfRange)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QCOMPARE(state->metadata.dataSourceProvenance, std::string{"kernel out of range"});
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(state->equatorial.declinationDeg));
}

void EphemerisFallbackValidationTests::outOfRangeSolarSystemRequestUsesSimpleFallbackWhenEnabled()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = EphemerisResultStatus::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "kernel out of range";

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(std::move(calculatorResult));
    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::Topocentric;

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QVERIFY(state->metadata.dataSourceProvenance.find("simple solar-system fallback") != std::string::npos);
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
}

void EphemerisFallbackValidationTests::failedRequestReturnsFailedStatus()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 1.0,
        .declinationDeg = 2.0,
    };
    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(std::move(calculatorResult));

    EphemerisRequest request = makeRequest();
    request.epoch.julianDatePart1 = std::numeric_limits<double>::quiet_NaN();

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::ComputationFailed));
    QVERIFY(!std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(!std::isfinite(state->equatorial.declinationDeg));
}

QTEST_APPLESS_MAIN(EphemerisFallbackValidationTests)

#include "EphemerisFallbackValidationTests.moc"
