#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/EphemerisDataSourceProvenance.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/IEphemerisFallbackStrategy.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/simple/SimpleEphemerisFallbackStrategy.hpp"
#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "UtcTimePoint.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

[[nodiscard]] OwnGalaxyCelestialBody makeSunBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "sun";
    body.displayName = "Sun";
    body.kind = BaseCelestialBody::Kind::Sun;
    return body;
}

[[nodiscard]] CelestialBodyCatalog makeSunCatalog()
{
    const std::array bodies{makeSunBody()};
    return CelestialBodyCatalog(std::span<const OwnGalaxyCelestialBody>{bodies});
}

[[nodiscard]] ObservationContext makeContext(const UtcTimePoint& utcTime)
{
    ObservationContext context;
    context.utcTime = utcTime;
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] EphemerisRequest
makeRequest(const AstronomicalEpoch& epoch, const UtcTimePoint& staleUtcTime, const bool fallbackToSimpleEngine = true)
{
    EphemerisRequest request;
    request.epoch = epoch;
    request.context = makeContext(staleUtcTime);
    request.options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());
    request.options.setFallbackToSimpleEngine(fallbackToSimpleEngine);
    return request;
}

[[nodiscard]] AstronomicalEpoch makeEpoch(const TimeScale timeScale)
{
    return AstronomicalEpoch{
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = timeScale,
    };
}

class DataOutOfRangeSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        static_cast<void>(input);

        HighPrecisionCalculatorResult result;
        result.metadata.status = EphemerisEngineQueryStatus::Type::OutOfRange;
        result.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
        result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
        result.metadata.dataSourceProvenance = "data-out-of-range fake";
        return result;
    }
};

class IdentityUtcTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.epoch = epoch.normalized();
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);

        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }
};

class FailingTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.epoch = epoch.normalized();
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);

        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }
};

[[nodiscard]] HighPrecisionEphemerisEngine::Dependencies
makeDependencies(std::shared_ptr<const ITimeScaleService> timeScaleService)
{
    HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.solarSystemStateCalculator = std::make_shared<DataOutOfRangeSolarSystemCalculator>();
    dependencies.timeScaleService = std::move(timeScaleService);
    dependencies.fallbackStrategy = std::make_shared<SimpleEphemerisFallbackStrategy>();
    dependencies.dataSetInfo.id = "test-data";
    dependencies.dataSetInfo.displayName = "Test data";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";
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

void assertDegradedFallbackMetadata(const CelestialBodyState& state)
{
    QCOMPARE(
        static_cast<std::uint8_t>(state.metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QCOMPARE(
        static_cast<std::uint8_t>(state.metadata.dataSourceProvenanceKind),
        static_cast<std::uint8_t>(EphemerisDataSourceProvenance::Type::SimpleSolarSystemFallback)
    );
    QVERIFY(std::isfinite(state.equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state.equatorial.declinationDeg));
}

void assertFailedFallbackMetadata(const CelestialBodyState& state)
{
    QCOMPARE(
        static_cast<std::uint8_t>(state.metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(!std::isfinite(state.equatorial.rightAscensionHours));
    QVERIFY(!std::isfinite(state.equatorial.declinationDeg));
}

}  // namespace

class EphemerisRuntimeFallbackTimeContractTests final : public QObject {
    Q_OBJECT

private slots:
    void fallbackUsesEpochNotStaleUtcForTdbEpoch();
    void fallbackUsesEpochNotStaleUtcForTtEpoch();
    void fallbackUsesEpochNotStaleUtcForUtcEpoch();
    void fallbackFailsWhenTimeScaleServiceIsMissing();
    void fallbackFailsWhenTimeScaleConversionFails();
    void disabledFallbackPreservesOutOfRangeResult();
};

void EphemerisRuntimeFallbackTimeContractTests::fallbackUsesEpochNotStaleUtcForTdbEpoch()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Tdb);
    const UtcTimePoint firstStaleUtc(std::chrono::seconds(0));
    const UtcTimePoint secondStaleUtc(std::chrono::seconds(2'000'000'000));
    const EphemerisRequest firstRequest = makeRequest(epoch, firstStaleUtc);
    const EphemerisRequest secondRequest = makeRequest(epoch, secondStaleUtc);

    const HighPrecisionEphemerisEngine engine(
        makeSunCatalog(), firstRequest.options, makeDependencies(std::make_shared<IdentityUtcTimeScaleService>())
    );

    const auto firstState = engine.computeBodyState(firstRequest, std::size_t{0});
    const auto secondState = engine.computeBodyState(secondRequest, std::size_t{0});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QCOMPARE(firstState->equatorial.rightAscensionHours, secondState->equatorial.rightAscensionHours);
    QCOMPARE(firstState->equatorial.declinationDeg, secondState->equatorial.declinationDeg);
    assertDegradedFallbackMetadata(*firstState);
    assertDegradedFallbackMetadata(*secondState);
}

void EphemerisRuntimeFallbackTimeContractTests::fallbackUsesEpochNotStaleUtcForTtEpoch()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Tt);
    const UtcTimePoint firstStaleUtc(std::chrono::seconds(0));
    const UtcTimePoint secondStaleUtc(std::chrono::seconds(2'000'000'000));
    const EphemerisRequest firstRequest = makeRequest(epoch, firstStaleUtc);
    const EphemerisRequest secondRequest = makeRequest(epoch, secondStaleUtc);

    const HighPrecisionEphemerisEngine engine(
        makeSunCatalog(), firstRequest.options, makeDependencies(std::make_shared<IdentityUtcTimeScaleService>())
    );

    const auto firstState = engine.computeBodyState(firstRequest, std::size_t{0});
    const auto secondState = engine.computeBodyState(secondRequest, std::size_t{0});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QCOMPARE(firstState->equatorial.rightAscensionHours, secondState->equatorial.rightAscensionHours);
    QCOMPARE(firstState->equatorial.declinationDeg, secondState->equatorial.declinationDeg);
    assertDegradedFallbackMetadata(*firstState);
    assertDegradedFallbackMetadata(*secondState);
}

void EphemerisRuntimeFallbackTimeContractTests::fallbackUsesEpochNotStaleUtcForUtcEpoch()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Utc);
    const UtcTimePoint firstStaleUtc(std::chrono::seconds(0));
    const UtcTimePoint secondStaleUtc(std::chrono::seconds(2'000'000'000));
    const EphemerisRequest firstRequest = makeRequest(epoch, firstStaleUtc);
    const EphemerisRequest secondRequest = makeRequest(epoch, secondStaleUtc);

    const HighPrecisionEphemerisEngine engine(
        makeSunCatalog(), firstRequest.options, makeDependencies(std::make_shared<IdentityUtcTimeScaleService>())
    );

    const auto firstState = engine.computeBodyState(firstRequest, std::size_t{0});
    const auto secondState = engine.computeBodyState(secondRequest, std::size_t{0});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QCOMPARE(firstState->equatorial.rightAscensionHours, secondState->equatorial.rightAscensionHours);
    QCOMPARE(firstState->equatorial.declinationDeg, secondState->equatorial.declinationDeg);
    assertDegradedFallbackMetadata(*firstState);
    assertDegradedFallbackMetadata(*secondState);
}

void EphemerisRuntimeFallbackTimeContractTests::fallbackFailsWhenTimeScaleServiceIsMissing()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Tdb);
    const EphemerisRequest request = makeRequest(epoch, UtcTimePoint(std::chrono::seconds(1'704'067'200)));

    const HighPrecisionEphemerisEngine engine(makeSunCatalog(), request.options, makeDependencies(nullptr));

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    assertFailedFallbackMetadata(*state);
}

void EphemerisRuntimeFallbackTimeContractTests::fallbackFailsWhenTimeScaleConversionFails()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Tdb);
    const EphemerisRequest request = makeRequest(epoch, UtcTimePoint(std::chrono::seconds(1'704'067'200)));

    const HighPrecisionEphemerisEngine engine(
        makeSunCatalog(), request.options, makeDependencies(std::make_shared<FailingTimeScaleService>())
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    assertFailedFallbackMetadata(*state);
}

void EphemerisRuntimeFallbackTimeContractTests::disabledFallbackPreservesOutOfRangeResult()
{
    const AstronomicalEpoch epoch = makeEpoch(TimeScale::Tdb);
    const EphemerisRequest request = makeRequest(epoch, UtcTimePoint(std::chrono::seconds(1'704'067'200)), false);

    const HighPrecisionEphemerisEngine engine(
        makeSunCatalog(), request.options, makeDependencies(std::make_shared<IdentityUtcTimeScaleService>())
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(!std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(!std::isfinite(state->equatorial.declinationDeg));
}

QTEST_APPLESS_MAIN(EphemerisRuntimeFallbackTimeContractTests)

#include "EphemerisRuntimeFallbackTimeContractTests.moc"
