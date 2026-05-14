#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "EphemerisFixtureSupport.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

constexpr int kNaifEarth = 399;
constexpr int kNaifMars = 499;
constexpr int kNaifSun = 10;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr double kCoordinateTolerance = 1.0e-9;

[[nodiscard]] CelestialBody makeMarsBody()
{
    return {
        .id = "mars",
        .displayName = "Mars",
        .type = CelestialBodyType::Planet,
        .ephemerisSource = CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] CelestialBody makeJupiterBody()
{
    return {
        .id = "jupiter",
        .displayName = "Jupiter",
        .type = CelestialBodyType::Planet,
        .ephemerisSource = CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] CelestialBody makeUnsupportedBody()
{
    return {
        .id = "ngc-test",
        .displayName = "NGC Test",
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

[[nodiscard]] AstronomicalEpoch makeEpoch(const double julianDatePart1 = 2'460'310.0)
{
    return {
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
}

[[nodiscard]] EphemerisRequest makeRequest(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = makeEpoch();
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = correctionFlags;
    request.options.enableAtmosphericRefraction =
        hasCorrectionFlag(correctionFlags, EphemerisCorrectionFlags::AtmosphericRefraction);
    return request;
}

[[nodiscard]] EphemerisDateRange
makeRange(std::string id, std::string displayName, const double startJd, const double endJd)
{
    return {
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = {.julianDatePart1 = startJd, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = endJd, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
}

[[nodiscard]] EphemerisDataSetInfo makeDataSetInfo(const bool includeLongRange)
{
    EphemerisDataSetInfo info;
    info.id = includeLongRange ? "acceptance-with-de441" : "acceptance-modern";
    info.displayName = includeLongRange ? "Acceptance modern and DE441" : "Acceptance bundled modern";
    info.version = "2026a";
    info.provenance = "acceptance test data";
    info.dateRanges.push_back(makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5));
    if (includeLongRange) {
        info.dateRanges.push_back(makeRange("de441-long-range", "Optional DE441 long range", -3'100'000.5, 8'000'000.5)
        );
    }
    return info;
}

[[nodiscard]] EphemerisEngineOptions makeOptions(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisEngineOptions options;
    options.engineKind = EphemerisEngineKind::HighPrecision;
    options.correctionFlags = correctionFlags;
    options.enableAtmosphericRefraction =
        hasCorrectionFlag(correctionFlags, EphemerisCorrectionFlags::AtmosphericRefraction);
    return options;
}

[[nodiscard]] bool nearlyEqual(const double lhs, const double rhs) noexcept
{
    return std::abs(lhs - rhs) <= kCoordinateTolerance;
}

class AcceptanceKernelProvider final : public ICalcephKernelProvider {
public:
    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++m_callCount;

        SolarSystemKernelStateResult result;
        result.metadata.dataSourceProvenance = "CALCEPH acceptance kernel";
        result.metadata.effectiveDataValidityRange =
            makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5);

        if (epoch.julianDatePart1 < 2'300'000.5 || epoch.julianDatePart1 > 2'700'000.5) {
            result.metadata.status = EphemerisResultStatus::OutOfRange;
            result.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
            result.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
            result.positionAu = SolarSystemKernelVector{.xAu = 1.0, .yAu = 0.0, .zAu = 0.0};
            return result;
        }

        if (targetNaifId == kNaifMars && centerNaifId == kNaifEarth) {
            result.positionAu = SolarSystemKernelVector{.xAu = 1.0, .yAu = 1.0, .zAu = 0.1};
            return result;
        }

        if (targetNaifId == kNaifEarth && centerNaifId == kNaifSolarSystemBarycenter) {
            result.positionAu = SolarSystemKernelVector{.xAu = 0.5, .yAu = 0.0, .zAu = 0.0};
            result.velocityAuPerDay = SolarSystemKernelVector{.xAu = 0.0, .yAu = 0.01, .zAu = 0.0};
            return result;
        }

        if (targetNaifId == kNaifMars && centerNaifId == kNaifSolarSystemBarycenter) {
            result.positionAu = SolarSystemKernelVector{.xAu = 1.25, .yAu = 1.0, .zAu = 0.1};
            return result;
        }

        if (targetNaifId == kNaifSun && centerNaifId == kNaifEarth) {
            result.positionAu = SolarSystemKernelVector{.xAu = -1.0, .yAu = 0.0, .zAu = 0.0};
            return result;
        }

        result.metadata.status = EphemerisResultStatus::Unsupported;
        result.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class CountingSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;

        HighPrecisionCalculatorResult result;
        result.equatorial = core::EquatorialCoordinate{
            .rightAscensionHours = 3.0 + static_cast<double>(input.bodyIndex),
            .declinationDeg = -2.0 + static_cast<double>(input.bodyIndex),
        };
        result.metadata.dataSourceProvenance = "acceptance counting calculator";
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::Geometric;
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class DegradedSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput&) const override
    {
        HighPrecisionCalculatorResult result;
        result.equatorial = core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
        result.metadata.status = EphemerisResultStatus::OutOfRange;
        result.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
        result.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
        result.metadata.dataSourceProvenance = "missing DE441 long-range kernel; bundled modern fallback";
        result.metadata.effectiveDataValidityRange =
            makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5);
        return result;
    }
};

[[nodiscard]] HighPrecisionEphemerisEngine makeHighPrecisionEngine(
    std::vector<CelestialBody> bodies,
    EphemerisEngineOptions options,
    HighPrecisionEphemerisEngineDependencies dependencies
)
{
    if (dependencies.dataSetInfo.id.empty()) {
        dependencies.dataSetInfo = makeDataSetInfo(false);
    }
    return HighPrecisionEphemerisEngine(bodies, options, std::move(dependencies));
}

}  // namespace

class EphemerisAcceptanceMatrixTests final : public QObject {
    Q_OBJECT

private slots:
    void factorySelectionStrictFailureAndFallbackRemainExplicit();
    void calcephProviderBackedSolarSystemRaDecSupportsCorrectionOptions();
    void absentLongRangeKernelProducesDegradedFallbackMetadata();
    void bundledAndOptionalLongRangeDataSetMetadataRemainDistinct();
    void deterministicHorizonsFixturesRemainReadable();
    void fullFrameComputationCacheAvoidsPerObjectRecompute();
};

void EphemerisAcceptanceMatrixTests::factorySelectionStrictFailureAndFallbackRemainExplicit()
{
    const std::array bodies{makeMarsBody()};

    EphemerisEngineFactoryRequest simpleRequest;
    simpleRequest.engineKind = EphemerisEngineKind::Simple;
    simpleRequest.catalogBodies = bodies;
    simpleRequest.options.engineKind = EphemerisEngineKind::Simple;
    const EphemerisEngineFactoryResult simpleResult = createEphemerisEngine(simpleRequest);
    QVERIFY(simpleResult.isSuccess());
    QVERIFY(simpleResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(simpleResult.engine->kind()), static_cast<std::uint8_t>(EphemerisEngineKind::Simple)
    );

    EphemerisEngineFactoryRequest strictRequest;
    strictRequest.engineKind = EphemerisEngineKind::HighPrecision;
    strictRequest.catalogBodies = bodies;
    strictRequest.options = makeOptions(EphemerisCorrectionFlags::ApparentTopocentric);
    strictRequest.fallbackPolicy = EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    const EphemerisEngineFactoryResult strictResult = createEphemerisEngine(strictRequest);
    QVERIFY(strictResult.isFailure());
    QVERIFY(strictResult.engine == nullptr);
    QVERIFY(strictResult.hasErrors());
    QVERIFY(!strictResult.diagnostics.empty());
    QVERIFY(!strictResult.diagnostics.front().displayText().empty());

    EphemerisEngineFactoryRequest fallbackRequest = strictRequest;
    fallbackRequest.fallbackPolicy = EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
    const EphemerisEngineFactoryResult fallbackResult = createEphemerisEngine(fallbackRequest);
    QVERIFY(fallbackResult.isSuccess());
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QVERIFY(fallbackResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.engine->kind()), static_cast<std::uint8_t>(EphemerisEngineKind::Simple)
    );
    QVERIFY(fallbackResult.hasDiagnostics());
    QVERIFY(!fallbackResult.hasErrors());
}

void EphemerisAcceptanceMatrixTests::calcephProviderBackedSolarSystemRaDecSupportsCorrectionOptions()
{
    auto kernelProvider = std::make_shared<AcceptanceKernelProvider>();
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.calcephKernelProvider = kernelProvider;
    dependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(kernelProvider);
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine =
        makeHighPrecisionEngine({makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::LightTime), dependencies);

    const auto geometricState = engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::Geometric), "mars");
    QVERIFY(geometricState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(geometricState->metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QCOMPARE(geometricState->metadata.dataSourceProvenance, std::string{"CALCEPH acceptance kernel"});

    const auto lightTimeState = engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::LightTime), "mars");
    QVERIFY(lightTimeState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(lightTimeState->metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QVERIFY(hasCorrectionFlag(lightTimeState->metadata.requestedCorrections, EphemerisCorrectionFlags::LightTime));
    QVERIFY(hasCorrectionFlag(lightTimeState->metadata.appliedCorrections, EphemerisCorrectionFlags::LightTime));
    QVERIFY(!nearlyEqual(geometricState->equatorial.rightAscensionHours, lightTimeState->equatorial.rightAscensionHours)
    );
    QVERIFY(kernelProvider->callCount() > 1);
}

void EphemerisAcceptanceMatrixTests::absentLongRangeKernelProducesDegradedFallbackMetadata()
{
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = std::make_shared<DegradedSolarSystemCalculator>();
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine =
        makeHighPrecisionEngine({makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::Geometric), dependencies);

    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Geometric);
    request.epoch = makeEpoch(-1'000'000.5);

    const auto state = engine.computeBodyState(request, "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QCOMPARE(
        state->metadata.dataSourceProvenance, std::string{"missing DE441 long-range kernel; bundled modern fallback"}
    );
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QCOMPARE(state->metadata.effectiveDataValidityRange->id, std::string{"de440-modern"});
}

void EphemerisAcceptanceMatrixTests::bundledAndOptionalLongRangeDataSetMetadataRemainDistinct()
{
    HighPrecisionEphemerisEngineDependencies bundledDependencies;
    bundledDependencies.solarSystemStateCalculator = std::make_shared<CountingSolarSystemCalculator>();
    bundledDependencies.dataSetInfo = makeDataSetInfo(false);
    const HighPrecisionEphemerisEngine bundledEngine = makeHighPrecisionEngine(
        {makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::Geometric), bundledDependencies
    );

    QCOMPARE(bundledEngine.dataSetInfo().id, std::string{"acceptance-modern"});
    QCOMPARE(bundledEngine.supportedDateRanges().size(), std::size_t{1});
    QCOMPARE(bundledEngine.supportedDateRanges()[0].id, std::string{"de440-modern"});

    HighPrecisionEphemerisEngineDependencies longRangeDependencies;
    longRangeDependencies.solarSystemStateCalculator = std::make_shared<CountingSolarSystemCalculator>();
    longRangeDependencies.dataSetInfo = makeDataSetInfo(true);
    const HighPrecisionEphemerisEngine longRangeEngine = makeHighPrecisionEngine(
        {makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::Geometric), longRangeDependencies
    );

    QCOMPARE(longRangeEngine.dataSetInfo().id, std::string{"acceptance-with-de441"});
    QCOMPARE(longRangeEngine.supportedDateRanges().size(), std::size_t{2});
    QCOMPARE(longRangeEngine.supportedDateRanges()[1].id, std::string{"de441-long-range"});
    QVERIFY(longRangeEngine.capabilities().supportsExtendedHistoricalRange);
}

void EphemerisAcceptanceMatrixTests::deterministicHorizonsFixturesRemainReadable()
{
    QString errorText;
    const auto fixture = skygate::ephemeris::tests::loadRaDecFixture(
        QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR)
            + QStringLiteral("/ephemeris/apparent_solar_system_mars_smoke.json"),
        &errorText
    );

    QVERIFY2(fixture.has_value(), qPrintable(errorText));
    QCOMPARE(fixture->metadata.source, QStringLiteral("JPL Horizons"));
    QVERIFY(fixture->metadata.apiParameters.contains(QStringLiteral("COMMAND='499'")));
    QVERIFY(fixture->metadata.target.contains(QStringLiteral("Mars")));
    QVERIFY(skygate::ephemeris::tests::hasFiniteCoordinates(fixture->expected));
    QVERIFY(fixture->toleranceDegrees > 0.0);
}

void EphemerisAcceptanceMatrixTests::fullFrameComputationCacheAvoidsPerObjectRecompute()
{
    auto calculator = std::make_shared<CountingSolarSystemCalculator>();
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = calculator;
    dependencies.computationCache = std::make_shared<EphemerisComputationCache>();
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(
        {makeMarsBody(), makeJupiterBody(), makeUnsupportedBody()},
        makeOptions(EphemerisCorrectionFlags::Geometric),
        dependencies
    );

    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Geometric);
    const SkySnapshot firstSnapshot = engine.compute(request);
    QCOMPARE(firstSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 2);

    const SkySnapshot cachedSnapshot = engine.compute(request);
    QCOMPARE(cachedSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 2);

    EphemerisRequest changedOptionsRequest = request;
    changedOptionsRequest.options.correctionFlags = EphemerisCorrectionFlags::LightTime;
    const SkySnapshot changedOptionsSnapshot = engine.compute(changedOptionsRequest);
    QCOMPARE(changedOptionsSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 4);
}

QTEST_APPLESS_MAIN(EphemerisAcceptanceMatrixTests)

#include "EphemerisAcceptanceMatrixTests.moc"
