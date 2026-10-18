#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisSnapshot.hpp"
#include "HighPrecisionEngineFixture.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PassThroughApparentPlaceCalculator.hpp"
#include "engine/EphemerisDatasetInfo.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using skygate::ephemeris::tests::HighPrecisionEngineFixture;
using skygate::ephemeris::tests::PassThroughApparentPlaceCalculator;

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

[[nodiscard]] OwnGalaxyCelestialBody makeAstrometricStarBody(std::string id, const double rightAscensionHours)
{
    OwnGalaxyCelestialBody body = HighPrecisionEngineFixture::fixedStarBody(std::move(id), rightAscensionHours, 10.0);
    body.fixedEquatorial.reset();
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            EquatorialCoordinate{
                .rightAscensionHours = rightAscensionHours,
                .declinationDeg = 10.0,
            },
        .referenceEpoch =
            skygate::core::AstronomicalEpoch{
                .julianDatePart1 = 2'451'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = skygate::core::TimeScale::Tdb,
            },
        .properMotionRightAscensionMasPerYear = 0.0,
        .properMotionDeclinationMasPerYear = 0.0,
        .stellarParallaxMas = 100.0,
        .radialVelocityKmPerSecond = 0.0,
    };
    return body;
}

class ThreadSafeEpochSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    explicit ThreadSafeEpochSolarSystemCalculator(const double baseRightAscensionHours)
        : m_baseRightAscensionHours(baseRightAscensionHours)
    {
    }

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        return HighPrecisionEngineFixture::calculatorResult(
            input, m_baseRightAscensionHours + input.request.epoch.julianDatePart2, -2.5, "thread-safe fake"
        );
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount.load();
    }

private:
    double m_baseRightAscensionHours = 0.0;
    mutable std::atomic<int> m_callCount = 0;
};

}  // namespace

class HighPrecisionEngineCacheTests final : public QObject {
    Q_OBJECT

private slots:
    void reusesFullFrameSnapshotsWithoutStaleResults();
    void reusesPreparedRequestStateForSingleBodies();
    void reusesCachedSingleBodyStatesWithoutFullSnapshots();
    void isolatesCachedSingleBodyStatesByBodyIndex();
    void servesConcurrentReadOnlyComputationsFromCache();
    void isolatesCachedSnapshotsByDataSetRevision();
    void isolatesCachedSnapshotsByMicrosecondUtc();
    void isolatesCachedSnapshotsByDateRangeContents();
    void isolatesCachedSnapshotsByCatalogContents();
    void doesNotReuseCachedSnapshotsForSignedZero();
    void findSnapshotBodyStateWithoutFullCopy();
};

void HighPrecisionEngineCacheTests::reusesFullFrameSnapshotsWithoutStaleResults()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(4.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create(), {}, computationCache
        )
    );

    EphemerisRequest firstRequest = HighPrecisionEngineFixture::request();
    firstRequest.epoch.julianDatePart2 = 0.25;
    const EphemerisSnapshot firstSnapshot = engine.compute(firstRequest);
    const EphemerisSnapshot repeatedSnapshot = engine.compute(firstRequest);

    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(firstSnapshot.states[0].equatorial.rightAscensionHours, 4.25);
    QCOMPARE(repeatedSnapshot.states[0].equatorial.rightAscensionHours, 4.25);

    EphemerisRequest changedRequest = firstRequest;
    changedRequest.epoch.julianDatePart2 = 0.75;
    const EphemerisSnapshot changedSnapshot = engine.compute(changedRequest);

    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(changedSnapshot.states[0].equatorial.rightAscensionHours, 4.75);

    const auto cachedState = engine.computeBodyState(firstRequest, std::size_t{0});

    QVERIFY(cachedState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(cachedState->equatorial.rightAscensionHours, 4.25);

    const auto cachedStateById = engine.computeBodyState(firstRequest, std::string_view{"sun"});

    QVERIFY(cachedStateById.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(cachedStateById->equatorial.rightAscensionHours, 4.25);
}

void HighPrecisionEngineCacheTests::reusesPreparedRequestStateForSingleBodies()
{
    const std::array bodies{makeAstrometricStarBody("star-a", 2.0), makeAstrometricStarBody("star-b", 5.0)};
    auto kernel = HighPrecisionEngineFixture::longRangeFallbackKernel();
    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>(kernel);
    auto computationCache = std::make_shared<EphemerisComputationCache>();
    HighPrecisionEphemerisEngine::Dependencies dependencies = HighPrecisionEngineFixture::dependencies(
        {}, starAstrometryCalculator, PassThroughApparentPlaceCalculator::create(), {}, computationCache
    );
    dependencies.calcephKernel = kernel;

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        std::move(dependencies)
    );

    const auto firstState = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::string_view{"star-a"});
    const auto secondState = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::string_view{"star-b"});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QCOMPARE(kernel->callCount(), 1);
}

void HighPrecisionEngineCacheTests::reusesCachedSingleBodyStatesWithoutFullSnapshots()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(9.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create(), {}, computationCache
        )
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch.julianDatePart2 = 0.125;
    const auto firstState = engine.computeBodyState(request, std::size_t{0});
    const auto repeatedState = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(firstState.has_value());
    QVERIFY(repeatedState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(firstState->equatorial.rightAscensionHours, 9.125);
    QCOMPARE(repeatedState->equatorial.rightAscensionHours, 9.125);

    EphemerisRequest changedRequest = request;
    changedRequest.epoch.julianDatePart2 = 0.375;
    const auto changedState = engine.computeBodyState(changedRequest, std::size_t{0});

    QVERIFY(changedState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(changedState->equatorial.rightAscensionHours, 9.375);

    const auto originalState = engine.computeBodyState(request, std::string_view{"sun"});

    QVERIFY(originalState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(originalState->equatorial.rightAscensionHours, 9.125);
}

void HighPrecisionEngineCacheTests::isolatesCachedSingleBodyStatesByBodyIndex()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody(), HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(11.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create(), {}, computationCache
        )
    );

    const EphemerisRequest request = HighPrecisionEngineFixture::request();
    const auto firstState = engine.computeBodyState(request, std::size_t{0});
    const auto secondState = engine.computeBodyState(request, std::size_t{1});
    const auto repeatedFirstState = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QVERIFY(repeatedFirstState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(firstState->bodyIndex, std::uint32_t{0});
    QCOMPARE(secondState->bodyIndex, std::uint32_t{1});
    QCOMPARE(repeatedFirstState->bodyIndex, std::uint32_t{0});
}

void HighPrecisionEngineCacheTests::servesConcurrentReadOnlyComputationsFromCache()
{
    constexpr std::size_t kThreadCount = 8U;
    constexpr int kComputesPerThread = 32;

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(7.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create(), {}, computationCache
        )
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch.julianDatePart2 = 0.125;
    const EphemerisSnapshot warmedSnapshot = engine.compute(request);
    QCOMPARE(warmedSnapshot.states[0].equatorial.rightAscensionHours, 7.125);
    QCOMPARE(solarSystemCalculator->callCount(), 1);

    std::array<bool, kThreadCount> threadResults{};
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (std::size_t threadIndex = 0U; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&engine, &request, &threadResults, threadIndex]() {
            bool matched = true;
            for (int iteration = 0; iteration < kComputesPerThread; ++iteration) {
                const EphemerisSnapshot snapshot = engine.compute(request);
                matched = matched && snapshot.states.size() == 1U
                          && snapshot.states[0].equatorial.rightAscensionHours == 7.125;
            }
            threadResults[threadIndex] = matched;
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    for (const bool threadResult : threadResults) {
        QVERIFY(threadResult);
    }
    QCOMPARE(solarSystemCalculator->callCount(), 1);
}

void HighPrecisionEngineCacheTests::isolatesCachedSnapshotsByDataSetRevision()
{
    EphemerisComputationCache computationCache;
    const std::vector<OwnGalaxyCelestialBody> bodies{HighPrecisionEngineFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const EphemerisRequest request = HighPrecisionEngineFixture::request();

    EphemerisDatasetInfo oldDataSet;
    oldDataSet.id = "test-data";
    oldDataSet.version = "old";
    oldDataSet.provenance = "old snapshot";

    EphemerisDatasetInfo newDataSet = oldDataSet;
    newDataSet.version = "new";
    newDataSet.provenance = "new snapshot";

    EphemerisSnapshot oldSnapshot;
    oldSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    EphemerisSnapshot newSnapshot;
    newSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
        }
    );

    computationCache.storeSnapshot(request, &catalog, oldDataSet, oldSnapshot);
    computationCache.storeSnapshot(request, &catalog, newDataSet, newSnapshot);

    const std::optional<EphemerisSnapshot> cachedOldSnapshot =
        computationCache.findSnapshot(request, &catalog, oldDataSet);
    const std::optional<EphemerisSnapshot> cachedNewSnapshot =
        computationCache.findSnapshot(request, &catalog, newDataSet);

    QVERIFY(cachedOldSnapshot.has_value());
    QVERIFY(cachedNewSnapshot.has_value());
    QCOMPARE(cachedOldSnapshot->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedNewSnapshot->states[0].equatorial.rightAscensionHours, 3.0);

    newSnapshot.states[0].equatorial.rightAscensionHours = 99.0;
    const std::optional<EphemerisSnapshot> cachedOldSnapshotAfterMutation =
        computationCache.findSnapshot(request, &catalog, oldDataSet);

    QVERIFY(cachedOldSnapshotAfterMutation.has_value());
    QCOMPARE(cachedOldSnapshotAfterMutation->states[0].equatorial.rightAscensionHours, 1.0);
}

void HighPrecisionEngineCacheTests::isolatesCachedSnapshotsByMicrosecondUtc()
{
    EphemerisComputationCache computationCache;
    const std::vector<OwnGalaxyCelestialBody> bodies{HighPrecisionEngineFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const EphemerisDatasetInfo dataSet;
    const EphemerisRequest baseRequest = HighPrecisionEngineFixture::request();
    EphemerisRequest shiftedRequest = baseRequest;
    shiftedRequest.context.utcTime += std::chrono::microseconds(500);

    EphemerisSnapshot baseSnapshot;
    baseSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    EphemerisSnapshot shiftedSnapshot;
    shiftedSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
        }
    );

    computationCache.storeSnapshot(baseRequest, &catalog, dataSet, baseSnapshot);
    computationCache.storeSnapshot(shiftedRequest, &catalog, dataSet, shiftedSnapshot);

    const std::optional<EphemerisSnapshot> cachedBaseSnapshot =
        computationCache.findSnapshot(baseRequest, &catalog, dataSet);
    const std::optional<EphemerisSnapshot> cachedShiftedSnapshot =
        computationCache.findSnapshot(shiftedRequest, &catalog, dataSet);

    QVERIFY(cachedBaseSnapshot.has_value());
    QVERIFY(cachedShiftedSnapshot.has_value());
    QCOMPARE(cachedBaseSnapshot->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedShiftedSnapshot->states[0].equatorial.rightAscensionHours, 3.0);
}

void HighPrecisionEngineCacheTests::isolatesCachedSnapshotsByDateRangeContents()
{
    EphemerisComputationCache computationCache;
    const std::vector<OwnGalaxyCelestialBody> bodies{HighPrecisionEngineFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const EphemerisRequest request = HighPrecisionEngineFixture::request();

    EphemerisDatasetInfo oldDataSet;
    oldDataSet.id = "test-data";
    oldDataSet.version = "same-version";
    oldDataSet.provenance = "same provenance";
    oldDataSet.dateRanges.push_back(
        EphemerisDateRange{
            .id = "modern",
            .displayName = "Modern",
            .start =
                {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
            .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        }
    );

    EphemerisDatasetInfo newDataSet = oldDataSet;
    newDataSet.dateRanges[0].end.julianDatePart1 = 2'600'000.5;

    EphemerisSnapshot oldSnapshot;
    oldSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    EphemerisSnapshot newSnapshot;
    newSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
        }
    );

    computationCache.storeSnapshot(request, &catalog, oldDataSet, oldSnapshot);
    computationCache.storeSnapshot(request, &catalog, newDataSet, newSnapshot);

    const std::optional<EphemerisSnapshot> cachedOldSnapshot =
        computationCache.findSnapshot(request, &catalog, oldDataSet);
    const std::optional<EphemerisSnapshot> cachedNewSnapshot =
        computationCache.findSnapshot(request, &catalog, newDataSet);

    QVERIFY(cachedOldSnapshot.has_value());
    QVERIFY(cachedNewSnapshot.has_value());
    QCOMPARE(cachedOldSnapshot->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedNewSnapshot->states[0].equatorial.rightAscensionHours, 3.0);
}

void HighPrecisionEngineCacheTests::isolatesCachedSnapshotsByCatalogContents()
{
    EphemerisComputationCache computationCache;
    std::vector<OwnGalaxyCelestialBody> bodies{HighPrecisionEngineFixture::fixedStarBody("star-a", 1.0, 2.0)};
    const CelestialBodyCatalog oldCatalog(bodies);
    const EphemerisRequest request = HighPrecisionEngineFixture::request();
    const EphemerisDatasetInfo dataSet = HighPrecisionEngineFixture::dependencies().dataSetInfo;

    EphemerisSnapshot oldSnapshot;
    oldSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    computationCache.storeSnapshot(request, &oldCatalog, dataSet, oldSnapshot);

    bodies[0] = HighPrecisionEngineFixture::fixedStarBody("star-b", 5.0, 6.0);
    const CelestialBodyCatalog newCatalog(bodies);
    const std::optional<EphemerisSnapshot> cachedSnapshot =
        computationCache.findSnapshot(request, &newCatalog, dataSet);

    QVERIFY(!cachedSnapshot.has_value());
}

void HighPrecisionEngineCacheTests::doesNotReuseCachedSnapshotsForSignedZero()
{
    EphemerisComputationCache computationCache;
    const std::vector<OwnGalaxyCelestialBody> bodies{HighPrecisionEngineFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const EphemerisDatasetInfo dataSet;

    EphemerisRequest positiveZeroRequest = HighPrecisionEngineFixture::request();
    positiveZeroRequest.epoch.julianDatePart2 = 0.0;
    positiveZeroRequest.context.observer.elevationMeters = 0.0;
    positiveZeroRequest.options.setAtmosphericTemperatureC(0.0);

    EphemerisRequest negativeZeroRequest = positiveZeroRequest;
    negativeZeroRequest.epoch.julianDatePart2 = -0.0;
    negativeZeroRequest.context.observer.elevationMeters = -0.0;
    negativeZeroRequest.options.setAtmosphericTemperatureC(-0.0);

    EphemerisSnapshot positiveZeroSnapshot;
    positiveZeroSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    EphemerisSnapshot negativeZeroSnapshot;
    negativeZeroSnapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
        }
    );

    computationCache.storeSnapshot(positiveZeroRequest, &catalog, dataSet, positiveZeroSnapshot);

    const std::optional<EphemerisSnapshot> cachedPositiveZero =
        computationCache.findSnapshot(positiveZeroRequest, &catalog, dataSet);

    QVERIFY(cachedPositiveZero.has_value());
    QCOMPARE(cachedPositiveZero->states[0].equatorial.rightAscensionHours, 1.0);
    QVERIFY(!computationCache.findSnapshot(negativeZeroRequest, &catalog, dataSet).has_value());
    QVERIFY(!computationCache.findSnapshotBodyState(negativeZeroRequest, &catalog, dataSet, 0U).has_value());

    computationCache.storeSnapshot(negativeZeroRequest, &catalog, dataSet, negativeZeroSnapshot);

    const std::optional<EphemerisSnapshot> cachedPositiveZeroAgain =
        computationCache.findSnapshot(positiveZeroRequest, &catalog, dataSet);
    const std::optional<EphemerisSnapshot> cachedNegativeZero =
        computationCache.findSnapshot(negativeZeroRequest, &catalog, dataSet);

    QVERIFY(cachedPositiveZeroAgain.has_value());
    QVERIFY(cachedNegativeZero.has_value());
    QCOMPARE(cachedPositiveZeroAgain->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedNegativeZero->states[0].equatorial.rightAscensionHours, 3.0);
}

void HighPrecisionEngineCacheTests::findSnapshotBodyStateWithoutFullCopy()
{
    EphemerisComputationCache computationCache;
    const std::vector<OwnGalaxyCelestialBody> bodies{
        HighPrecisionEngineFixture::sunBody(), HighPrecisionEngineFixture::starBody()
    };
    const CelestialBodyCatalog catalog(bodies);
    const EphemerisRequest request = HighPrecisionEngineFixture::request();
    const EphemerisDatasetInfo dataSet = HighPrecisionEngineFixture::dependencies().dataSetInfo;

    EphemerisSnapshot snapshot;
    snapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
        }
    );
    snapshot.states.push_back(
        CelestialBodyState{
            .bodyIndex = 1U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
        }
    );
    computationCache.storeSnapshot(request, &catalog, dataSet, snapshot);

    const std::optional<CelestialBodyState> secondBody =
        computationCache.findSnapshotBodyState(request, &catalog, dataSet, 1U);

    QVERIFY(secondBody.has_value());
    QCOMPARE(secondBody->bodyIndex, std::uint32_t{1});
    QCOMPARE(secondBody->equatorial.rightAscensionHours, 3.0);
    QCOMPARE(secondBody->equatorial.declinationDeg, 4.0);
    QVERIFY(!computationCache.findSnapshotBodyState(request, &catalog, dataSet, 2U).has_value());
}

QTEST_APPLESS_MAIN(HighPrecisionEngineCacheTests)

#include "HighPrecisionEngineCacheTests.moc"
