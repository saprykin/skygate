#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include "skygate/ephemeris/EarthOrientationProvider.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] CelestialBody makeStarBody()
{
    return {
        .id = "vega",
        .displayName = "Vega",
        .type = CelestialBodyType::Star,
        .ephemerisSource = CelestialBodyEphemerisSource::Star,
    };
}

[[nodiscard]] CelestialBody makeUnsupportedBody()
{
    return {
        .id = "unresolved",
        .displayName = "Unresolved",
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
    request.options.correctionFlags = EphemerisCorrectionFlags::Apparent;
    return request;
}

[[nodiscard]] HighPrecisionCalculatorResult makeCalculatorResult(
    const HighPrecisionComputationInput& input,
    const double rightAscensionHours,
    const double declinationDeg,
    const std::string_view provenance
)
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    result.metadata.dataSourceProvenance = provenance;
    result.metadata.appliedCorrections = input.request.options.correctionFlags;
    return result;
}

class RecordingSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;
        return makeCalculatorResult(input, 1.25, -2.5, "solar-system fake");
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class RecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;
        return makeCalculatorResult(input, 3.5, 42.0, "star-astrometry fake");
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class RecordingApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_callCount;
        m_lastFlags = input.request.options.correctionFlags;

        HighPrecisionCalculatorResult result = calculatorResult;
        if (result.equatorial.has_value()) {
            result.equatorial->rightAscensionHours += 10.0;
        }
        result.metadata.appliedCorrections = input.request.options.correctionFlags;
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class RecordingResultBuilder final : public IEphemerisResultBuilder {
public:
    [[nodiscard]] CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_stateCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;

        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        if (calculatorResult.equatorial.has_value()) {
            state.equatorial = *calculatorResult.equatorial;
        }
        if (calculatorResult.horizontal.has_value()) {
            state.horizontal = *calculatorResult.horizontal;
        }
        state.metadata = calculatorResult.metadata;
        state.metadata.estimatedAngularUncertaintyArcsec = 0.42;
        return state;
    }

    [[nodiscard]] CelestialBodyState buildUnsupportedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_unsupportedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = EphemerisResultStatus::Unsupported;
        state.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_failedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = EphemerisResultStatus::Failed;
        state.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] int stateCount() const noexcept
    {
        return m_stateCount;
    }

    [[nodiscard]] int unsupportedCount() const noexcept
    {
        return m_unsupportedCount;
    }

    [[nodiscard]] int failedCount() const noexcept
    {
        return m_failedCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    [[nodiscard]] static CelestialBodyState makeEmptyState(const std::size_t bodyIndex) noexcept
    {
        CelestialBodyState state;
        state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
        state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
        state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
        return state;
    }

    mutable int m_stateCount = 0;
    mutable int m_unsupportedCount = 0;
    mutable int m_failedCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class StubEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    [[nodiscard]] const EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const EarthOrientationTableEntry> entries() const noexcept override
    {
        return {};
    }

private:
    EarthOrientationDataInfo m_dataInfo;
};

class StubAtmosphericRefractionCalculator final : public IAtmosphericRefractionCalculator {};

[[nodiscard]] HighPrecisionEphemerisEngineDependencies makeDependencies(
    std::shared_ptr<RecordingSolarSystemCalculator> solarSystemCalculator = {},
    std::shared_ptr<RecordingStarAstrometryCalculator> starAstrometryCalculator = {},
    std::shared_ptr<RecordingApparentPlaceCalculator> apparentPlaceCalculator = {},
    std::shared_ptr<RecordingResultBuilder> resultBuilder = {}
)
{
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = std::move(solarSystemCalculator);
    dependencies.starAstrometryCalculator = std::move(starAstrometryCalculator);
    dependencies.apparentPlaceCalculator = std::move(apparentPlaceCalculator);
    dependencies.resultBuilder = std::move(resultBuilder);
    dependencies.dataSetInfo.id = "test-data";
    dependencies.dataSetInfo.displayName = "Test data";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";
    dependencies.dataSetInfo.dateRanges.push_back(EphemerisDateRange{
        .id = "modern",
        .displayName = "Modern",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    });
    return dependencies;
}

}  // namespace

class HighPrecisionEphemerisEngineTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesMetadataAndCapabilities();
    void dispatchesSolarSystemAndStarBodies();
    void forwardsOptionsThroughCollaboratorsAndResultBuilder();
    void bypassesApparentPlaceForGeometricSolarSystemRequests();
    void validatesRequestsBeforeDispatchingCalculators();
    void returnsStructuredUnsupportedStatus();
};

void HighPrecisionEphemerisEngineTests::exposesMetadataAndCapabilities()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto dependencies = makeDependencies(solarSystemCalculator, starAstrometryCalculator);
    dependencies.earthOrientationProvider = std::make_shared<StubEarthOrientationProvider>();
    dependencies.atmosphericRefractionCalculator = std::make_shared<StubAtmosphericRefractionCalculator>();

    EphemerisEngineOptions options;
    options.engineKind = EphemerisEngineKind::Simple;
    options.correctionFlags = EphemerisCorrectionFlags::ApparentTopocentric;

    const HighPrecisionEphemerisEngine engine(bodies, options, std::move(dependencies));

    QCOMPARE(static_cast<std::uint8_t>(engine.kind()), static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision));
    QVERIFY(engine.name() == std::string_view{"High-precision ephemeris engine"});
    QCOMPARE(
        static_cast<std::uint8_t>(engine.options().engineKind),
        static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision)
    );

    const EphemerisCapabilities capabilities = engine.capabilities();
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision)
    );
    QVERIFY(capabilities.supportsSolarSystemBodies);
    QVERIFY(capabilities.supportsCatalogStars);
    QVERIFY(capabilities.supportsTopocentricPositions);
    QVERIFY(capabilities.supportsAtmosphericRefraction);
    QVERIFY(capabilities.supportsExtendedHistoricalRange);
    QCOMPARE(engine.supportedDateRanges().size(), std::size_t{1});
    QVERIFY(engine.dataSetInfo().id == std::string{"test-data"});
}

void HighPrecisionEphemerisEngineTests::dispatchesSolarSystemAndStarBodies()
{
    const std::array bodies{makeSunBody(), makeStarBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeDependencies(solarSystemCalculator, starAstrometryCalculator, apparentPlaceCalculator, resultBuilder)
    );

    const SkySnapshot snapshot = engine.compute(makeRequest());

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(starAstrometryCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 2);
    QCOMPARE(resultBuilder->stateCount(), 2);
    QVERIFY(solarSystemCalculator->lastBodyId() == std::string{"sun"});
    QVERIFY(starAstrometryCalculator->lastBodyId() == std::string{"vega"});
    QCOMPARE(snapshot.states[0].equatorial.rightAscensionHours, 11.25);
    QCOMPARE(snapshot.states[0].equatorial.declinationDeg, -2.5);
    QCOMPARE(snapshot.states[1].equatorial.rightAscensionHours, 13.5);
    QCOMPARE(snapshot.states[1].equatorial.declinationDeg, 42.0);
}

void HighPrecisionEphemerisEngineTests::forwardsOptionsThroughCollaboratorsAndResultBuilder()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::EarthOrientation;

    EphemerisEngineOptions engineOptions = request.options;
    engineOptions.correctionFlags = EphemerisCorrectionFlags::AtmosphericRefraction;

    const HighPrecisionEphemerisEngine engine(
        bodies, engineOptions, makeDependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint32_t>(solarSystemCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(apparentPlaceCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(resultBuilder->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QVERIFY(state->metadata.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*state->metadata.estimatedAngularUncertaintyArcsec, 0.42);
}

void HighPrecisionEphemerisEngineTests::bypassesApparentPlaceForGeometricSolarSystemRequests()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;

    const HighPrecisionEphemerisEngine engine(
        bodies, request.options, makeDependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->stateCount(), 1);
    QCOMPARE(state->equatorial.rightAscensionHours, 1.25);
    QCOMPARE(state->equatorial.declinationDeg, -2.5);
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::Geometric)
    );
}

void HighPrecisionEphemerisEngineTests::validatesRequestsBeforeDispatchingCalculators()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies(solarSystemCalculator, {}, {}, resultBuilder)
    );

    EphemerisRequest request = makeRequest();
    request.epoch.julianDatePart1 = std::numeric_limits<double>::quiet_NaN();

    const auto state = engine.computeBodyState(request, "sun");

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->failedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::ComputationFailed));
}

void HighPrecisionEphemerisEngineTests::returnsStructuredUnsupportedStatus()
{
    const std::array bodies{makeUnsupportedBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeDependencies(solarSystemCalculator, starAstrometryCalculator, {}, resultBuilder)
    );

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(starAstrometryCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->unsupportedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Unsupported)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::UnsupportedBody));
    QVERIFY(!state->metadata.dataSourceProvenance.empty());
}

QTEST_APPLESS_MAIN(HighPrecisionEphemerisEngineTests)

#include "HighPrecisionEphemerisEngineTests.moc"
