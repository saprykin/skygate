#include "CelestialBodyCatalog.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "EphemerisRequestFactory.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogFactory.hpp"
#include "engine/EphemerisEngineQueries.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"
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

}  // namespace

class EphemerisEngineContractTests final : public QObject {
    Q_OBJECT

private slots:
    void simpleEngineHonorsHardenedContract();
    void testDoublesHonorHardenedContract();
    void contextQueriesRouteThroughDirectSingleBodyOverloads();
#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS
    void highPrecisionEngineHonorsHardenedContract();
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
#endif

QTEST_APPLESS_MAIN(EphemerisEngineContractTests)

#include "EphemerisEngineContractTests.moc"
