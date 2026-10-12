#include "CelestialBodyCatalog.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "app/SkyContextController.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "render/SkyRenderPoint.hpp"
#include "scene/SkySceneModel.hpp"
#include "support/GuardApparentPlaceCalculator.hpp"
#include "support/GuardBatchStarAstrometryCalculator.hpp"
#include "support/PerformanceGuardChecks.hpp"
#include "support/PerformanceGuardScenario.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "skygate/testsupport/PerformanceBudget.hpp"

#include <QElapsedTimer>
#include <QtTest>

#include <memory>
#include <optional>
#include <vector>

using skygate::ui::tests::PerformanceGuardChecks;
using skygate::ui::tests::PerformanceGuardScenario;

class PerformanceSceneProfileTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsLargeSceneWithinGuardrail();
    void buildsHighPrecisionLargeCatalogWithinGuardrail();
    void profilesHighPrecisionLargeFixedCatalogSelection();
};

namespace {

constexpr qint64 kLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kHighPrecisionCacheKeyBudgetMs = 2000;

void verifyHighPrecisionCounterDeltas(
    const skygate::ui::tests::GuardBatchStarAstrometryCalculator& starAstrometryCalculator,
    const skygate::ui::tests::GuardApparentPlaceCalculator& apparentPlaceCalculator,
    const int astrometryBatchCallsBefore,
    const int astrometrySingleCallsBefore,
    const int apparentBatchCallsBefore,
    const int apparentSingleCallsBefore,
    const int expectedAstrometryBatchDelta,
    const int expectedAstrometrySingleDelta,
    const int expectedApparentBatchDelta,
    const int expectedApparentSingleDelta,
    const char* operationName
)
{
    PerformanceGuardChecks::verifyCounterDelta(
        astrometryBatchCallsBefore,
        starAstrometryCalculator.batchCallCount(),
        expectedAstrometryBatchDelta,
        "astrometry batch",
        operationName
    );
    PerformanceGuardChecks::verifyCounterDelta(
        astrometrySingleCallsBefore,
        starAstrometryCalculator.singleCallCount(),
        expectedAstrometrySingleDelta,
        "astrometry single",
        operationName
    );
    PerformanceGuardChecks::verifyCounterDelta(
        apparentBatchCallsBefore,
        apparentPlaceCalculator.batchCallCount(),
        expectedApparentBatchDelta,
        "apparent batch",
        operationName
    );
    PerformanceGuardChecks::verifyCounterDelta(
        apparentSingleCallsBefore,
        apparentPlaceCalculator.singleCallCount(),
        expectedApparentSingleDelta,
        "apparent single",
        operationName
    );
}

}  // namespace

void PerformanceSceneProfileTests::buildsLargeSceneWithinGuardrail()
{
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromCatalog(
        PerformanceGuardScenario::makeLargeMixedCatalog()
    );
    QVERIFY(catalog != nullptr);
    auto engineResult = skygate::ephemeris::EphemerisEngineFactory::create(*catalog);
    QVERIFY(engineResult.isSuccess());
    auto engine = std::move(engineResult.engine);
    QVERIFY(engine != nullptr);

    SkyContextController controller(
        std::move(catalog), std::move(engine), PerformanceGuardScenario::initializationOptions(), nullptr
    );
    controller.setLatitudeText(QStringLiteral("47.4"));
    controller.setLongitudeText(QStringLiteral("8.5"));
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2026-05-03"), QStringLiteral("21:00:00")));
    controller.setMagnitudeCutoff(12.0);
    controller.setViewCenter(45.0, 180.0);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);

    QElapsedTimer timer;
    timer.start();
    sceneModel.setViewportSize(1280.0, 800.0);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(sceneModel.snapshotGeneration() > 0U);
    QVERIFY(
        !sceneModel.renderPointSpan().empty() || !sceneModel.renderGlyphSpan().empty()
        || !sceneModel.renderLineSpan().empty()
    );
    PerformanceGuardChecks::verifyElapsedBelow(elapsedMs, kLargeSceneBuildBudgetMs, "large scene build");
}

void PerformanceSceneProfileTests::buildsHighPrecisionLargeCatalogWithinGuardrail()
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies =
        PerformanceGuardScenario::makeHighPrecisionGuardCatalog();
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    QVERIFY(catalog != nullptr);
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(bodies);

    auto starAstrometryCalculator = std::make_shared<skygate::ui::tests::GuardBatchStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<skygate::ui::tests::GuardApparentPlaceCalculator>();
    auto computationCache = std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();

    skygate::ephemeris::EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());

    skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.starAstrometryCalculator = starAstrometryCalculator;
    dependencies.apparentPlaceCalculator = apparentPlaceCalculator;
    dependencies.computationCache = computationCache;
    dependencies.dataSetInfo = PerformanceGuardScenario::makeHighPrecisionGuardDataSetInfo();

    auto engine = std::make_unique<skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine>(
        bodyCatalog, options, dependencies
    );

    SkyContextController::InitializationOptions initializationOptions =
        PerformanceGuardScenario::initializationOptions();
    initializationOptions.rebuildEphemerisEngineOnStartup = false;
    SkyContextController controller(std::move(catalog), std::move(engine), initializationOptions, nullptr);
    controller.setLatitudeText(QStringLiteral("47.4"));
    controller.setLongitudeText(QStringLiteral("8.5"));
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2026-05-03"), QStringLiteral("21:00:00")));
    controller.setMagnitudeCutoff(12.0);
    controller.setViewCenter(45.0, 180.0);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);

    QElapsedTimer timer;
    timer.start();
    sceneModel.setViewportSize(1280.0, 800.0);
    const qint64 firstBuildElapsedMs = timer.elapsed();

    QVERIFY(sceneModel.snapshotGeneration() > 0U);
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(starAstrometryCalculator->lastBatchSize(), bodies.size());
    QCOMPARE(apparentPlaceCalculator->batchCallCount(), 1);
    QCOMPARE(apparentPlaceCalculator->singleCallCount(), 0);
    PerformanceGuardChecks::verifyElapsedBelow(
        firstBuildElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog scene build"
    );

    skygate::ephemeris::EphemerisSnapshot cacheSnapshot;
    cacheSnapshot.states.push_back(skygate::ephemeris::CelestialBodyState{.bodyIndex = 0U});
    timer.restart();
    computationCache->storeSnapshot(
        controller.ephemerisRequestContext().request, &bodyCatalog, dependencies.dataSetInfo, cacheSnapshot
    );
    const std::optional<skygate::ephemeris::EphemerisSnapshot> cachedSnapshot = computationCache->findSnapshot(
        controller.ephemerisRequestContext().request, &bodyCatalog, dependencies.dataSetInfo
    );
    const qint64 cacheElapsedMs = timer.elapsed();

    QVERIFY(cachedSnapshot.has_value());
    PerformanceGuardChecks::verifyElapsedBelow(
        cacheElapsedMs, kHighPrecisionCacheKeyBudgetMs, "high precision large cache key store and lookup"
    );

    timer.restart();
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2026-05-03"), QStringLiteral("21:00:10")));
    const qint64 timeUpdateElapsedMs = timer.elapsed();

    QCOMPARE(starAstrometryCalculator->batchCallCount(), 2);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    PerformanceGuardChecks::verifyElapsedBelow(
        timeUpdateElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog time update"
    );
}

void PerformanceSceneProfileTests::profilesHighPrecisionLargeFixedCatalogSelection()
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies =
        PerformanceGuardScenario::makeHighPrecisionGuardCatalog();
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    QVERIFY(catalog != nullptr);
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(bodies);

    auto starAstrometryCalculator = std::make_shared<skygate::ui::tests::GuardBatchStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<skygate::ui::tests::GuardApparentPlaceCalculator>();
    auto computationCache = std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();

    skygate::ephemeris::EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());

    skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.starAstrometryCalculator = starAstrometryCalculator;
    dependencies.apparentPlaceCalculator = apparentPlaceCalculator;
    dependencies.computationCache = computationCache;
    dependencies.dataSetInfo = PerformanceGuardScenario::makeHighPrecisionGuardDataSetInfo();

    auto engine = std::make_unique<skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine>(
        bodyCatalog, options, dependencies
    );

    SkyContextController::InitializationOptions initializationOptions =
        PerformanceGuardScenario::initializationOptions();
    initializationOptions.rebuildEphemerisEngineOnStartup = false;
    SkyContextController controller(std::move(catalog), std::move(engine), initializationOptions, nullptr);
    controller.setLatitudeText(QStringLiteral("47.4"));
    controller.setLongitudeText(QStringLiteral("8.5"));
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2026-05-03"), QStringLiteral("21:00:00")));
    controller.setMagnitudeCutoff(12.0);
    controller.setViewCenter(45.0, 180.0);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1280.0, 800.0);
    QVERIFY(sceneModel.snapshotGeneration() > 0U);

    const SkyRenderPoint* targetPoint = nullptr;
    for (const SkyRenderPoint& point : sceneModel.renderPointSpan()) {
        targetPoint = &point;
        break;
    }
    QVERIFY(targetPoint != nullptr);

    const int astrometryBatchCallsBefore = starAstrometryCalculator->batchCallCount();
    const int astrometrySingleCallsBefore = starAstrometryCalculator->singleCallCount();
    const int apparentBatchCallsBefore = apparentPlaceCalculator->batchCallCount();
    const int apparentSingleCallsBefore = apparentPlaceCalculator->singleCallCount();

    QElapsedTimer timer;
    timer.start();
    QVERIFY(sceneModel.selectObjectAt(targetPoint->x, targetPoint->y));
    const qint64 selectElapsedMs = timer.elapsed();

    qInfo().noquote() << skygate::testsupport::performanceMetricMessage(
        selectElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog selection",
        skygate::testsupport::isStrictPerformanceGuardMode()
    );
    qInfo().noquote() << QStringLiteral(
                             "perf high precision selection counters: astrometryBatchDelta=%1 "
                             "astrometrySingleDelta=%2 apparentBatchDelta=%3 apparentSingleDelta=%4"
    )
                             .arg(starAstrometryCalculator->batchCallCount() - astrometryBatchCallsBefore)
                             .arg(starAstrometryCalculator->singleCallCount() - astrometrySingleCallsBefore)
                             .arg(apparentPlaceCalculator->batchCallCount() - apparentBatchCallsBefore)
                             .arg(apparentPlaceCalculator->singleCallCount() - apparentSingleCallsBefore);
    // Selecting a fixed star samples its trail through the engine for the
    // full window at the ten-minute render step. The present sample is served
    // from the scene snapshot cache, so the remaining samples account for
    // 144 single-body computations plus the one inspector detail recompute.
    verifyHighPrecisionCounterDeltas(
        *starAstrometryCalculator,
        *apparentPlaceCalculator,
        astrometryBatchCallsBefore,
        astrometrySingleCallsBefore,
        apparentBatchCallsBefore,
        apparentSingleCallsBefore,
        0,
        145,
        0,
        145,
        "high precision large fixed catalog selection"
    );

    QVERIFY(sceneModel.selectedObjectInspector().value("visible").toBool());

    const int moveAstrometryBatchCallsBefore = starAstrometryCalculator->batchCallCount();
    const int moveAstrometrySingleCallsBefore = starAstrometryCalculator->singleCallCount();
    const int moveApparentBatchCallsBefore = apparentPlaceCalculator->batchCallCount();
    const int moveApparentSingleCallsBefore = apparentPlaceCalculator->singleCallCount();

    timer.restart();
    sceneModel.moveSelectedObjectInspector(240.0, 180.0);
    const qint64 moveElapsedMs = timer.elapsed();

    qInfo().noquote() << skygate::testsupport::performanceMetricMessage(
        moveElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog inspector move",
        skygate::testsupport::isStrictPerformanceGuardMode()
    );
    qInfo().noquote() << QStringLiteral(
                             "perf high precision inspector move counters: astrometryBatchDelta=%1 "
                             "astrometrySingleDelta=%2 apparentBatchDelta=%3 apparentSingleDelta=%4"
    )
                             .arg(starAstrometryCalculator->batchCallCount() - moveAstrometryBatchCallsBefore)
                             .arg(starAstrometryCalculator->singleCallCount() - moveAstrometrySingleCallsBefore)
                             .arg(apparentPlaceCalculator->batchCallCount() - moveApparentBatchCallsBefore)
                             .arg(apparentPlaceCalculator->singleCallCount() - moveApparentSingleCallsBefore);
    verifyHighPrecisionCounterDeltas(
        *starAstrometryCalculator,
        *apparentPlaceCalculator,
        moveAstrometryBatchCallsBefore,
        moveAstrometrySingleCallsBefore,
        moveApparentBatchCallsBefore,
        moveApparentSingleCallsBefore,
        0,
        0,
        0,
        0,
        "high precision large fixed catalog inspector move"
    );

    const int panAstrometryBatchCallsBefore = starAstrometryCalculator->batchCallCount();
    const int panAstrometrySingleCallsBefore = starAstrometryCalculator->singleCallCount();
    const int panApparentBatchCallsBefore = apparentPlaceCalculator->batchCallCount();
    const int panApparentSingleCallsBefore = apparentPlaceCalculator->singleCallCount();

    timer.restart();
    controller.panViewBy(1.0, 1.0);
    const qint64 panElapsedMs = timer.elapsed();

    qInfo().noquote() << skygate::testsupport::performanceMetricMessage(
        panElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog trail pan",
        skygate::testsupport::isStrictPerformanceGuardMode()
    );
    qInfo().noquote() << QStringLiteral(
                             "perf high precision trail pan counters: astrometryBatchDelta=%1 "
                             "astrometrySingleDelta=%2 apparentBatchDelta=%3 apparentSingleDelta=%4"
    )
                             .arg(starAstrometryCalculator->batchCallCount() - panAstrometryBatchCallsBefore)
                             .arg(starAstrometryCalculator->singleCallCount() - panAstrometrySingleCallsBefore)
                             .arg(apparentPlaceCalculator->batchCallCount() - panApparentBatchCallsBefore)
                             .arg(apparentPlaceCalculator->singleCallCount() - panApparentSingleCallsBefore);
    verifyHighPrecisionCounterDeltas(
        *starAstrometryCalculator,
        *apparentPlaceCalculator,
        panAstrometryBatchCallsBefore,
        panAstrometrySingleCallsBefore,
        panApparentBatchCallsBefore,
        panApparentSingleCallsBefore,
        0,
        0,
        0,
        0,
        "high precision large fixed catalog trail pan"
    );

    const int zoomAstrometryBatchCallsBefore = starAstrometryCalculator->batchCallCount();
    const int zoomAstrometrySingleCallsBefore = starAstrometryCalculator->singleCallCount();
    const int zoomApparentBatchCallsBefore = apparentPlaceCalculator->batchCallCount();
    const int zoomApparentSingleCallsBefore = apparentPlaceCalculator->singleCallCount();

    timer.restart();
    controller.zoomViewByScaleDelta(1.2);
    const qint64 zoomElapsedMs = timer.elapsed();

    qInfo().noquote() << skygate::testsupport::performanceMetricMessage(
        zoomElapsedMs,
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision large fixed catalog trail zoom",
        skygate::testsupport::isStrictPerformanceGuardMode()
    );
    qInfo().noquote() << QStringLiteral(
                             "perf high precision trail zoom counters: astrometryBatchDelta=%1 "
                             "astrometrySingleDelta=%2 apparentBatchDelta=%3 apparentSingleDelta=%4"
    )
                             .arg(starAstrometryCalculator->batchCallCount() - zoomAstrometryBatchCallsBefore)
                             .arg(starAstrometryCalculator->singleCallCount() - zoomAstrometrySingleCallsBefore)
                             .arg(apparentPlaceCalculator->batchCallCount() - zoomApparentBatchCallsBefore)
                             .arg(apparentPlaceCalculator->singleCallCount() - zoomApparentSingleCallsBefore);
    verifyHighPrecisionCounterDeltas(
        *starAstrometryCalculator,
        *apparentPlaceCalculator,
        zoomAstrometryBatchCallsBefore,
        zoomAstrometrySingleCallsBefore,
        zoomApparentBatchCallsBefore,
        zoomApparentSingleCallsBefore,
        0,
        0,
        0,
        0,
        "high precision large fixed catalog trail zoom"
    );
}

QTEST_GUILESS_MAIN(PerformanceSceneProfileTests)

#include "PerformanceSceneProfileTests.moc"
