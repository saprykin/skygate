#include "CelestialBodyCatalog.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "EphemerisRequestFactory.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogFactory.hpp"
#include "engine/EphemerisEngineQueries.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/highprecision/IStarAstrometryCalculator.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace skygate::core;
using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

[[nodiscard]] OwnGalaxyCelestialBody makeFixedStarBody(std::string id)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 6.0,
        .declinationDeg = 20.0,
    };
    return body;
}

[[nodiscard]] ObservationContext makeContext()
{
    ObservationContext context;
    context.observer = {.latitudeDeg = 47.3769, .longitudeDeg = 8.5417, .elevationMeters = 408.0};
    context.utcTime = UtcTimePoint(std::chrono::seconds(1'704'067'200));
    return context;
}

void verifyCommonContract(const IEphemerisEngine& engine)
{
    QVERIFY(!engine.name().empty());

    const EphemerisCapabilities knownCapabilities =
        EphemerisCapabilities::solarSystemBodies() | EphemerisCapabilities::catalogStars()
        | EphemerisCapabilities::topocentricPositions() | EphemerisCapabilities::atmosphericRefraction()
        | EphemerisCapabilities::extendedHistoricalRange();
    const std::uint8_t knownMask = knownCapabilities.bits();
    QVERIFY((engine.capabilities().bits() & static_cast<std::uint8_t>(~knownMask)) == 0U);

    const EphemerisEngineOptions options = engine.options();
    QCOMPARE(options.engineKind(), engine.kind());

    const EphemerisRequest request = EphemerisRequestFactory::requestFromContext(makeContext(), options);
    QVERIFY(!engine.computeBodyState(request, "definitely_missing").has_value());
    QVERIFY(!engine.computeBodyState(request, std::size_t{1'000'000}).has_value());
}

void verifyEquivalentStates(const CelestialBodyState& lhs, const CelestialBodyState& rhs)
{
    QCOMPARE(lhs.bodyIndex, rhs.bodyIndex);
    QCOMPARE(lhs.metadata.status, rhs.metadata.status);

    if (std::isfinite(lhs.equatorial.rightAscensionHours)) {
        QCOMPARE(lhs.equatorial.rightAscensionHours, rhs.equatorial.rightAscensionHours);
        QCOMPARE(lhs.equatorial.declinationDeg, rhs.equatorial.declinationDeg);
    } else {
        QVERIFY(!std::isfinite(rhs.equatorial.rightAscensionHours));
        QVERIFY(!std::isfinite(rhs.equatorial.declinationDeg));
    }

    if (std::isfinite(lhs.horizontal.altitudeDeg)) {
        QCOMPARE(lhs.horizontal.altitudeDeg, rhs.horizontal.altitudeDeg);
        QCOMPARE(lhs.horizontal.azimuthDeg, rhs.horizontal.azimuthDeg);
    } else {
        QVERIFY(!std::isfinite(rhs.horizontal.altitudeDeg));
        QVERIFY(!std::isfinite(rhs.horizontal.azimuthDeg));
    }
}

void verifyExplicitRequestAndConvenienceEquivalence(const IEphemerisEngine& engine, const std::string_view bodyId)
{
    const ObservationContext context = makeContext();
    const EphemerisRequest request = EphemerisRequestFactory::requestFromContext(context, engine.options());

    const auto convenience = engine.computeBodyState(context, bodyId);
    const auto explicitRequest = engine.computeBodyState(request, bodyId);
    QCOMPARE(convenience.has_value(), explicitRequest.has_value());
    if (convenience.has_value()) {
        verifyEquivalentStates(*convenience, *explicitRequest);
    }
}

[[nodiscard]] std::unique_ptr<IStarCatalog> makeSingleBodyCatalog(const std::string_view bodyId)
{
    return CatalogFactory::createStarCatalogFromBodies(
        std::vector<OwnGalaxyCelestialBody>{makeFixedStarBody(std::string{bodyId})}
    );
}

#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS

[[nodiscard]] HighPrecisionCalculatorResult makeContractCalculatorResult(
    const HighPrecisionComputationInput& input,
    const double rightAscensionHours,
    const double declinationDeg,
    const std::string_view provenance
)
{
    HighPrecisionCalculatorResult result;
    result.equatorial = EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    result.metadata.dataSourceProvenance = provenance;
    result.metadata.appliedCorrections = input.request.options.correctionFlags();
    return result;
}

class ContractSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    ContractSolarSystemCalculator() = default;
    explicit ContractSolarSystemCalculator(HighPrecisionCalculatorResult result)
        : m_result(std::move(result)), m_hasCustomResult(true)
    {
    }

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        if (m_hasCustomResult) {
            return m_result;
        }
        return makeContractCalculatorResult(input, 1.25, -2.5, "contract solar-system fake");
    }

private:
    HighPrecisionCalculatorResult m_result;
    bool m_hasCustomResult = false;
};

class ContractStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        return makeContractCalculatorResult(input, 3.5, 42.0, "contract star-astrometry fake");
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        static_cast<void>(request);
        static_cast<void>(arrays);
        static_cast<void>(preparedRequestState);
        return {};
    }
};

class ContractIdentityTimeScaleService final : public ITimeScaleService {
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

[[nodiscard]] OwnGalaxyCelestialBody makeContractSunBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "sun";
    body.displayName = "Sun";
    body.kind = BaseCelestialBody::Kind::Sun;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeContractUnsupportedBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "unresolved";
    body.displayName = "Unresolved";
    body.kind = BaseCelestialBody::Kind::Constellation;
    return body;
}

[[nodiscard]] EphemerisEngineOptions makeContractHighPrecisionOptions()
{
    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());
    options.setFallbackToSimpleEngine(false);
    return options;
}

[[nodiscard]] HighPrecisionEphemerisEngine::Dependencies makeContractDependencies()
{
    HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.solarSystemStateCalculator = std::make_shared<ContractSolarSystemCalculator>();
    dependencies.starAstrometryCalculator = std::make_shared<ContractStarAstrometryCalculator>();
    dependencies.timeScaleService = std::make_shared<ContractIdentityTimeScaleService>();
    dependencies.dataSetInfo.id = "contract-hp";
    dependencies.dataSetInfo.displayName = "Contract high precision";
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

#endif

}  // namespace

class EphemerisEngineContractTests final : public QObject {
    Q_OBJECT

private slots:
    void simpleEngineHonorsHardenedContract();
    void testDoublesHonorHardenedContract();
    void contextQueriesRouteThroughDirectSingleBodyOverloads();
#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS
    void highPrecisionEngineHonorsHardenedContract();
    void providerBackedHighPrecisionEngineHonorsHardenedContract();
    void providerBackedHighPrecisionEngineReportsUnsupportedBodies();
    void providerBackedHighPrecisionEngineReportsOutOfRangeData();
#endif
};

void EphemerisEngineContractTests::simpleEngineHonorsHardenedContract()
{
    auto catalog = makeSingleBodyCatalog("target");
    QVERIFY(catalog != nullptr);

    auto result = EphemerisEngineFactory::create(*catalog);
    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    const IEphemerisEngine& engine = *result.engine;

    verifyCommonContract(engine);
    verifyExplicitRequestAndConvenienceEquivalence(engine, "target");

    EphemerisRequest unsupportedRequest;
    unsupportedRequest.context = makeContext();
    unsupportedRequest.epoch = {
        .julianDatePart1 = 2'460'000.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tt,
    };
    unsupportedRequest.options = engine.options();

    const auto unsupportedState = engine.computeBodyState(unsupportedRequest, "target");
    QVERIFY(unsupportedState.has_value());
    QVERIFY(!unsupportedState->metadata.isSuccessful());
    QCOMPARE(unsupportedState->metadata.status, EphemerisEngineQueryStatus::Type::Unsupported);
}

void EphemerisEngineContractTests::testDoublesHonorHardenedContract()
{
    const tests::FixedAltitudeEngine fixedAltitudeEngine(0.0);
    verifyCommonContract(fixedAltitudeEngine);
    verifyExplicitRequestAndConvenienceEquivalence(fixedAltitudeEngine, "target");

    auto catalog = makeSingleBodyCatalog("target");
    QVERIFY(catalog != nullptr);
    auto innerResult = EphemerisEngineFactory::create(*catalog);
    QVERIFY(innerResult.isSuccess());
    const EphemerisEngineOptions innerOptions = innerResult.engine->options();
    tests::RequestCountingEphemerisEngine countingEngine(
        std::move(innerResult.engine), innerOptions, std::shared_ptr<const CelestialBodyCatalog>()
    );
    verifyCommonContract(countingEngine);
    verifyExplicitRequestAndConvenienceEquivalence(countingEngine, "target");

    const tests::NonEnumeratedHighPrecisionTraitsEngine nonEnumeratedEngine;
    verifyCommonContract(nonEnumeratedEngine);
    verifyExplicitRequestAndConvenienceEquivalence(nonEnumeratedEngine, "target");
}

void EphemerisEngineContractTests::contextQueriesRouteThroughDirectSingleBodyOverloads()
{
    auto catalog = makeSingleBodyCatalog("target");
    QVERIFY(catalog != nullptr);
    auto innerResult = EphemerisEngineFactory::create(*catalog);
    QVERIFY(innerResult.isSuccess());
    const EphemerisEngineOptions innerOptions = innerResult.engine->options();
    tests::RequestCountingEphemerisEngine engine(
        std::move(innerResult.engine), innerOptions, std::shared_ptr<const CelestialBodyCatalog>()
    );

    const ObservationContext context = makeContext();

    const auto byId = EphemerisEngineQueries::computeBodyStateById(engine, context, "target");
    QVERIFY(byId.has_value());
    QCOMPARE(engine.contextSampleCount(), 1);
    QCOMPARE(engine.requestSampleCount(), 0);

    const auto byIndex = EphemerisEngineQueries::computeBodyStateByIndex(engine, context, std::size_t{0});
    QVERIFY(byIndex.has_value());
    QCOMPARE(engine.contextSampleCount(), 2);
    QCOMPARE(engine.requestSampleCount(), 0);
}

#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS
void EphemerisEngineContractTests::highPrecisionEngineHonorsHardenedContract()
{
    CelestialBodyCatalog catalog(std::vector<OwnGalaxyCelestialBody>{makeFixedStarBody("target")});

    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
    options.setFallbackToSimpleEngine(false);

    highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.dataSetInfo.id = "contract-test";
    dependencies.dataSetInfo.displayName = "Contract test";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";

    const highprecision::HighPrecisionEphemerisEngine engine(catalog, options, std::move(dependencies));

    verifyCommonContract(engine);
    verifyExplicitRequestAndConvenienceEquivalence(engine, "target");

    EphemerisRequest unsupportedRequest;
    unsupportedRequest.context = makeContext();
    unsupportedRequest.options = engine.options();

    const auto snapshot = engine.compute(unsupportedRequest);
    QVERIFY(!snapshot.states.empty());
    QVERIFY(!snapshot.states.front().metadata.isSuccessful());
}

void EphemerisEngineContractTests::providerBackedHighPrecisionEngineHonorsHardenedContract()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makeContractSunBody(),
        makeFixedStarBody("target"),
    };
    CelestialBodyCatalog catalog(bodies);

    const EphemerisEngineOptions options = makeContractHighPrecisionOptions();
    const highprecision::HighPrecisionEphemerisEngine engine(catalog, options, makeContractDependencies());

    verifyCommonContract(engine);
    verifyExplicitRequestAndConvenienceEquivalence(engine, "target");
    verifyExplicitRequestAndConvenienceEquivalence(engine, "sun");

    const ObservationContext context = makeContext();
    const EphemerisRequest request = EphemerisRequestFactory::requestFromContext(context, engine.options());

    const EphemerisSnapshot snapshot = engine.compute(request);
    QCOMPARE(snapshot.states.size(), std::size_t{2});
    for (const CelestialBodyState& state : snapshot.states) {
        QVERIFY(std::isfinite(state.equatorial.rightAscensionHours));
        QVERIFY(std::isfinite(state.equatorial.declinationDeg));
        QCOMPARE(
            static_cast<std::uint8_t>(state.metadata.status),
            static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
        );
    }

    const auto targetById = engine.computeBodyState(context, "target");
    const auto targetByRequest = engine.computeBodyState(request, "target");

    QVERIFY(targetById.has_value());
    QVERIFY(targetByRequest.has_value());
    verifyEquivalentStates(*targetById, *targetByRequest);
    QCOMPARE(targetById->equatorial.rightAscensionHours, 3.5);
    QCOMPARE(targetById->equatorial.declinationDeg, 42.0);

    QCOMPARE(
        static_cast<std::uint8_t>(targetById->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(targetById->metadata.dataSourceProvenance == std::string{"contract star-astrometry fake"});
    QCOMPARE(targetById->metadata.requestedCorrections, options.correctionFlags());
    QCOMPARE(targetById->metadata.appliedCorrections, options.correctionFlags());
    QCOMPARE(targetById->metadata.skippedCorrections, EphemerisCorrectionFlags::noCorrections());
    QCOMPARE(targetById->metadata.unavailableCorrections, EphemerisCorrectionFlags::noCorrections());

    const EphemerisDatasetInfo dataSetInfo = engine.dataSetInfo();
    QVERIFY(dataSetInfo.id == std::string{"contract-hp"});
    QVERIFY(dataSetInfo.displayName == std::string{"Contract high precision"});
    QVERIFY(dataSetInfo.version == std::string{"fixture"});
    QVERIFY(dataSetInfo.provenance == std::string{"unit test"});
    QCOMPARE(dataSetInfo.dateRanges.size(), std::size_t{1});
    QCOMPARE(engine.supportedDateRanges().size(), std::size_t{1});

    QVERIFY(!engine.computeBodyState(context, "unknown").has_value());
    QVERIFY(!engine.computeBodyState(context, std::size_t{1'000'000}).has_value());
}

void EphemerisEngineContractTests::providerBackedHighPrecisionEngineReportsUnsupportedBodies()
{
    CelestialBodyCatalog catalog(std::vector<OwnGalaxyCelestialBody>{makeContractUnsupportedBody()});

    const EphemerisEngineOptions options = makeContractHighPrecisionOptions();
    const highprecision::HighPrecisionEphemerisEngine engine(catalog, options, makeContractDependencies());

    const auto state = engine.computeBodyState(makeContext(), "unresolved");

    QVERIFY(state.has_value());
    QCOMPARE(state->bodyIndex, std::uint32_t{0});
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedBody));
    QVERIFY(!state->metadata.dataSourceProvenance.empty());
    QVERIFY(!std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(!std::isfinite(state->equatorial.declinationDeg));
}

void EphemerisEngineContractTests::providerBackedHighPrecisionEngineReportsOutOfRangeData()
{
    HighPrecisionCalculatorResult outOfRangeResult;
    outOfRangeResult.metadata.status = EphemerisEngineQueryStatus::Type::OutOfRange;
    outOfRangeResult.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    outOfRangeResult.metadata.dataSourceProvenance = "contract out-of-range fake";

    highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.solarSystemStateCalculator =
        std::make_shared<ContractSolarSystemCalculator>(std::move(outOfRangeResult));
    dependencies.timeScaleService = std::make_shared<ContractIdentityTimeScaleService>();
    dependencies.dataSetInfo.id = "contract-out-of-range";
    dependencies.dataSetInfo.displayName = "Contract out of range";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";

    const EphemerisEngineOptions options = makeContractHighPrecisionOptions();
    CelestialBodyCatalog catalog(std::vector<OwnGalaxyCelestialBody>{makeContractSunBody()});
    const highprecision::HighPrecisionEphemerisEngine engine(catalog, options, std::move(dependencies));

    const auto state = engine.computeBodyState(makeContext(), "sun");

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"contract out-of-range fake"});
    QVERIFY(!std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(!std::isfinite(state->equatorial.declinationDeg));
}
#endif

QTEST_APPLESS_MAIN(EphemerisEngineContractTests)

#include "EphemerisEngineContractTests.moc"
