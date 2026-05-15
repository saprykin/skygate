#include <QtTest>

#include "LocationCatalogModel.hpp"
#include "MacDockIcon.hpp"
#include "SkyContextController.hpp"
#include "SkyHitTargetIndex.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "SkySceneModel.hpp"
#include "SkyTheme.hpp"

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <QElapsedTimer>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PerformanceGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsLargeSceneWithinGuardrail();
    void buildsHighPrecisionLargeFixedCatalogWithinGuardrail();
    void searchesLargeMixedCatalogWithinGuardrail();
    void hitTestsDenseRenderFrameWithinGuardrail();
    void buildsManyObjectTrailsWithinGuardrail();
    void packagedResourceDependenciesResolve();
    void platformNativeHooksTolerateEmptyInputs();
};

namespace {

constexpr qint64 kLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kHighPrecisionLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kHighPrecisionCacheKeyBudgetMs = 2000;
constexpr qint64 kLargeSearchLoadBudgetMs = 10000;
constexpr qint64 kLargeSearchQueryBudgetMs = 3000;
constexpr qint64 kDenseHitTestBudgetMs = 5000;
constexpr qint64 kManyTrailsBudgetMs = 12000;

bool isStrictPerformanceGuardMode()
{
    const QString mode = QString::fromUtf8(qgetenv("SKYGATE_PERFORMANCE_GUARD_MODE")).trimmed().toLower();
    const QString legacyStrict = QString::fromUtf8(qgetenv("SKYGATE_STRICT_PERFORMANCE_GUARDS")).trimmed().toLower();
    return mode == QStringLiteral("strict")
           || (!legacyStrict.isEmpty() && legacyStrict != QStringLiteral("0") && legacyStrict != QStringLiteral("false")
               && legacyStrict != QStringLiteral("no") && legacyStrict != QStringLiteral("off"));
}

QString performanceMetricMessage(
    const qint64 elapsedMs, const qint64 budgetMs, const char* operationName, const bool strictMode
)
{
    const qint64 deltaMs = budgetMs - elapsedMs;
    const double budgetPercent =
        budgetMs > 0 ? (static_cast<double>(elapsedMs) * 100.0 / static_cast<double>(budgetMs)) : 0.0;
    return QStringLiteral(
               "perf %1: %2 elapsed=%3 ms budget=%4 ms delta=%5 ms budget_used=%6% "
               "strict=%7"
    )
        .arg(elapsedMs < budgetMs ? QStringLiteral("within-budget") : QStringLiteral("over-budget"))
        .arg(QString::fromUtf8(operationName))
        .arg(elapsedMs)
        .arg(budgetMs)
        .arg(deltaMs)
        .arg(QString::number(budgetPercent, 'f', 1))
        .arg(strictMode ? QStringLiteral("on") : QStringLiteral("off"));
}

class PerformanceTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        return computeBodyState(request.context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        (void)context;
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        const double offsetMinutes = static_cast<double>(
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count()
        );
        const double bodyOffset = static_cast<double>(bodyIndex % 40U) * 0.015;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = bodyIndex,
            .horizontal = {
                .altitudeDeg = 45.0 + bodyOffset + (offsetMinutes / 8000.0),
                .azimuthDeg = 180.0 + bodyOffset + (offsetMinutes / 8000.0)
            }
        };
    }
};

class GuardBatchStarAstrometryCalculator final : public skygate::ephemeris::highprecision::IStarAstrometryCalculator {
public:
    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult
    calculate(const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input) const override
    {
        ++m_singleCallCount;
        skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result;
        result.equatorial = input.body.fixedEquatorial;
        result.metadata.status = skygate::ephemeris::EphemerisResultStatus::Valid;
        result.metadata.dataSourceProvenance = "performance guard single fallback";
        return result;
    }

    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculateBatch(
        const skygate::ephemeris::EphemerisRequest& request,
        const skygate::ephemeris::highprecision::CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState =
            {}
    ) const override
    {
        static_cast<void>(request);
        static_cast<void>(preparedRequestState);

        ++m_batchCallCount;
        m_lastBatchSize = arrays.size();

        std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> results;
        results.reserve(arrays.size());
        for (std::size_t arrayIndex = 0; arrayIndex < arrays.size(); ++arrayIndex) {
            skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result;
            result.equatorial = arrays.referenceEquatorial(arrayIndex);
            result.metadata.status = skygate::ephemeris::EphemerisResultStatus::Valid;
            result.metadata.dataSourceProvenance = "performance guard batch astrometry";
            results.push_back(
                skygate::ephemeris::highprecision::StarAstrometryBatchResult{
                    .bodyIndex = arrays.bodyIndices()[arrayIndex],
                    .result = result,
                }
            );
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return m_singleCallCount;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

private:
    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

class GuardApparentPlaceCalculator final : public skygate::ephemeris::highprecision::IApparentPlaceCalculator {
public:
    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult apply(
        const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input,
        const skygate::ephemeris::highprecision::HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_singleCallCount;
        skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result = calculatorResult;
        result.horizontal = horizontalForBodyIndex(input.bodyIndex);
        result.metadata.appliedCorrections = input.request.options.correctionFlags;
        return result;
    }

    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> applyBatch(
        const skygate::ephemeris::EphemerisRequest& request,
        std::span<const skygate::ephemeris::CelestialBody> bodies,
        std::span<const skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState =
            {}
    ) const override
    {
        static_cast<void>(bodies);
        static_cast<void>(preparedRequestState);

        ++m_batchCallCount;
        m_lastBatchSize = calculatorResults.size();

        std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> results;
        results.reserve(calculatorResults.size());
        for (const skygate::ephemeris::highprecision::StarAstrometryBatchResult& calculatorResult : calculatorResults) {
            skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result = calculatorResult.result;
            result.horizontal = horizontalForBodyIndex(calculatorResult.bodyIndex);
            result.metadata.appliedCorrections = request.options.correctionFlags;
            results.push_back(
                skygate::ephemeris::highprecision::StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = result,
                }
            );
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return m_singleCallCount;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

private:
    [[nodiscard]] static skygate::core::HorizontalCoordinate
    horizontalForBodyIndex(const std::size_t bodyIndex) noexcept
    {
        return skygate::core::HorizontalCoordinate{
            .altitudeDeg = -20.0 + static_cast<double>(bodyIndex % 90U),
            .azimuthDeg = std::fmod(static_cast<double>(bodyIndex) * 0.37, 360.0),
        };
    }

    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

void verifyElapsedBelow(const qint64 elapsedMs, const qint64 budgetMs, const char* operationName)
{
    const bool strictMode = isStrictPerformanceGuardMode();
    const QString message = performanceMetricMessage(elapsedMs, budgetMs, operationName, strictMode);

    if (elapsedMs < budgetMs) {
        qInfo().noquote() << message;
        return;
    }

    const QString strictHint = QStringLiteral(
                                   "%1; set SKYGATE_PERFORMANCE_GUARD_MODE=strict or "
                                   "SKYGATE_STRICT_PERFORMANCE_GUARDS=1 to make advisory budgets fail"
    )
                                   .arg(message);
    if (!strictMode) {
        qWarning().noquote() << strictHint;
        return;
    }

    QVERIFY2(false, qPrintable(strictHint));
}

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours, .declinationDeg = declinationDeg
    };
    if (type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
        body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
            .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
            .aliases =
                {"Guard Alias " + body.id,
                 "Shared Collision Alias "
                     + std::to_string(static_cast<int>(std::fmod(rightAscensionHours * 1000.0, 25.0)))},
            .majorAxisArcmin = 5.0,
            .minorAxisArcmin = 3.0,
            .positionAngleDeg = 0.0
        };
    }
    return body;
}

std::vector<skygate::ephemeris::CelestialBody> makeLargeMixedCatalog()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.reserve(9000U);
    for (int index = 0; index < 7000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.017, 24.0);
        const double declinationDeg = -72.0 + static_cast<double>(index % 145);
        const double magnitude = 2.0 + static_cast<double>(index % 80) / 10.0;
        bodies.push_back(makeBody(
            "guard_star_" + std::to_string(index),
            "Guard Star " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::Star,
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }

    for (int index = 0; index < 2000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.071, 24.0);
        const double declinationDeg = -55.0 + static_cast<double>(index % 110);
        const double magnitude = 4.0 + static_cast<double>(index % 90) / 10.0;
        bodies.push_back(makeBody(
            "guard_dso_" + std::to_string(index),
            "Guard Galaxy " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::DeepSkyObject,
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }
    return bodies;
}

skygate::ephemeris::CelestialBody makeHighPrecisionGuardBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::CelestialBody body =
        makeBody(std::move(id), std::move(displayName), type, visualMagnitude, rightAscensionHours, declinationDeg);
    body.ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial;
    return body;
}

std::vector<skygate::ephemeris::CelestialBody> makeHighPrecisionGuardCatalog()
{
    constexpr int kHygScaleStarCount = 119626;
    constexpr int kOpenNgcScaleDeepSkyCount = 13308;

    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.reserve(static_cast<std::size_t>(kHygScaleStarCount + kOpenNgcScaleDeepSkyCount));
    for (int index = 0; index < kHygScaleStarCount; ++index) {
        bodies.push_back(makeHighPrecisionGuardBody(
            "hp_guard_star_" + std::to_string(index),
            "HP Guard Star " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::Star,
            2.0 + static_cast<double>(index % 100) / 10.0,
            std::fmod(static_cast<double>(index) * 0.011, 24.0),
            -75.0 + static_cast<double>(index % 150)
        ));
    }
    for (int index = 0; index < kOpenNgcScaleDeepSkyCount; ++index) {
        bodies.push_back(makeHighPrecisionGuardBody(
            "hp_guard_dso_" + std::to_string(index),
            "HP Guard Galaxy " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::DeepSkyObject,
            4.0 + static_cast<double>(index % 80) / 10.0,
            std::fmod(static_cast<double>(index) * 0.073, 24.0),
            -55.0 + static_cast<double>(index % 110)
        ));
    }
    return bodies;
}

skygate::ephemeris::EphemerisDataSetInfo makeHighPrecisionGuardDataSetInfo()
{
    skygate::ephemeris::EphemerisDataSetInfo dataSetInfo;
    dataSetInfo.id = "performance-guard-data";
    dataSetInfo.displayName = "Performance guard data";
    dataSetInfo.version = "1";
    dataSetInfo.provenance = "synthetic performance guard";
    dataSetInfo.dateRanges.push_back(
        skygate::ephemeris::EphemerisDateRange{
            .id = "guard-range",
            .displayName = "Guard range",
            .start =
                {.julianDatePart1 = 2'400'000.5,
                 .julianDatePart2 = 0.0,
                 .timeScale = skygate::ephemeris::TimeScale::Tdb},
            .end = {
                .julianDatePart1 = 2'700'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
            },
        }
    );
    return dataSetInfo;
}

SkyContextController::InitializationOptions testInitializationOptions()
{
    return SkyContextController::InitializationOptions{.loadSettings = false, .initializeLocation = false};
}

skygate::ephemeris::SkySnapshot makeHitTestSnapshot(const int bodyCount)
{
    skygate::ephemeris::SkySnapshot snapshot;
    auto bodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>();
    bodies->reserve(static_cast<std::size_t>(bodyCount));
    for (int index = 0; index < bodyCount; ++index) {
        bodies->push_back(makeBody(
            "hit_body_" + std::to_string(index),
            "Hit Body " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::Star,
            5.0,
            0.0,
            0.0
        ));
    }
    snapshot.catalogBodies = std::move(bodies);
    return snapshot;
}

skygate::ui::internal::SkyThemeRenderPalette makeTrailRenderTheme()
{
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    renderTheme.selectionMarkerBorder = QColor("#80c0ff");
    return renderTheme;
}

}  // namespace

void PerformanceGuardTests::buildsLargeSceneWithinGuardrail()
{
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(makeLargeMixedCatalog());
    QVERIFY(catalog != nullptr);
    auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    SkyContextController controller(std::move(catalog), std::move(engine), testInitializationOptions(), nullptr);
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
    verifyElapsedBelow(elapsedMs, kLargeSceneBuildBudgetMs, "large scene build");
}

void PerformanceGuardTests::buildsHighPrecisionLargeFixedCatalogWithinGuardrail()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeHighPrecisionGuardCatalog();
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(bodies);
    QVERIFY(catalog != nullptr);

    auto starAstrometryCalculator = std::make_shared<GuardBatchStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<GuardApparentPlaceCalculator>();
    auto computationCache = std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();

    skygate::ephemeris::EphemerisEngineOptions options;
    options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent;

    skygate::ephemeris::highprecision::HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.starAstrometryCalculator = starAstrometryCalculator;
    dependencies.apparentPlaceCalculator = apparentPlaceCalculator;
    dependencies.computationCache = computationCache;
    dependencies.dataSetInfo = makeHighPrecisionGuardDataSetInfo();

    auto engine = std::make_unique<skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine>(
        bodies, options, dependencies
    );

    SkyContextController::InitializationOptions initializationOptions = testInitializationOptions();
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
    verifyElapsedBelow(
        firstBuildElapsedMs, kHighPrecisionLargeSceneBuildBudgetMs, "high precision large fixed catalog scene build"
    );

    skygate::ephemeris::SkySnapshot cacheSnapshot;
    cacheSnapshot.states.push_back(skygate::ephemeris::CelestialBodyState{.bodyIndex = 0U});
    timer.restart();
    computationCache->storeSnapshot(
        controller.ephemerisRequestContext().request, bodies, dependencies.dataSetInfo, cacheSnapshot
    );
    const std::optional<skygate::ephemeris::SkySnapshot> cachedSnapshot =
        computationCache->findSnapshot(controller.ephemerisRequestContext().request, bodies, dependencies.dataSetInfo);
    const qint64 cacheElapsedMs = timer.elapsed();

    QVERIFY(cachedSnapshot.has_value());
    verifyElapsedBelow(
        cacheElapsedMs, kHighPrecisionCacheKeyBudgetMs, "high precision large cache key store and lookup"
    );

    timer.restart();
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2026-05-03"), QStringLiteral("21:00:10")));
    const qint64 timeUpdateElapsedMs = timer.elapsed();

    QCOMPARE(starAstrometryCalculator->batchCallCount(), 2);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    verifyElapsedBelow(
        timeUpdateElapsedMs, kHighPrecisionLargeSceneBuildBudgetMs, "high precision large fixed catalog time update"
    );
}

void PerformanceGuardTests::searchesLargeMixedCatalogWithinGuardrail()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeLargeMixedCatalog();
    bodies.push_back(makeBody(
        "guard_exact_target", "Guard Exact Target", skygate::ephemeris::CelestialBodyType::Star, -1.0, 6.0, -16.0
    ));

    SkyObjectSearchModel model;
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs{
        {"Guard Constellation", {"guard_star_1", "guard_star_2"}}
    };
    QElapsedTimer timer;
    timer.start();
    model.setCatalogData(bodies, labelRefs);
    const qint64 loadElapsedMs = timer.elapsed();
    verifyElapsedBelow(loadElapsedMs, kLargeSearchLoadBudgetMs, "large search catalog load");

    timer.restart();
    model.setFilterText(QStringLiteral("guard exact target"));
    const qint64 exactQueryElapsedMs = timer.elapsed();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("guard_exact_target")
    );
    verifyElapsedBelow(exactQueryElapsedMs, kLargeSearchQueryBudgetMs, "large exact search");

    timer.restart();
    model.setFilterText(QStringLiteral("shared collision alias 7"));
    const qint64 aliasQueryElapsedMs = timer.elapsed();
    QVERIFY(model.rowCount() > 0);
    verifyElapsedBelow(aliasQueryElapsedMs, kLargeSearchQueryBudgetMs, "large alias search");
}

void PerformanceGuardTests::hitTestsDenseRenderFrameWithinGuardrail()
{
    constexpr int kTargetCount = 15000;
    const auto snapshot = makeHitTestSnapshot(kTargetCount);
    SkyRenderFrame frame;
    frame.points.reserve(kTargetCount);
    for (int index = 0; index < kTargetCount; ++index) {
        frame.points.push_back(
            SkyRenderPoint{
                .x = 10.0 + static_cast<double>((index * 37) % 1180),
                .y = 10.0 + static_cast<double>((index * 53) % 780),
                .sizePx = 3.0,
                .bodyIndex = static_cast<std::uint32_t>(index)
            }
        );
    }

    SkyHitTargetIndex hitIndex;
    QElapsedTimer timer;
    timer.start();
    hitIndex.rebuild(frame, snapshot);
    int hitCount = 0;
    for (int index = 0; index < 2000; ++index) {
        const auto hit = hitIndex.bodyIndexAt(
            10.0 + static_cast<double>((index * 37) % 1180),
            10.0 + static_cast<double>((index * 53) % 780),
            1200.0,
            800.0,
            snapshot
        );
        if (hit.has_value()) {
            ++hitCount;
        }
    }
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(hitCount > 0);
    verifyElapsedBelow(elapsedMs, kDenseHitTestBudgetMs, "dense hit testing");
}

void PerformanceGuardTests::buildsManyObjectTrailsWithinGuardrail()
{
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, 90.0)
    );
    QVERIFY(projection.has_value());
    const PerformanceTrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;

    SkyObjectTrailInput input;
    input.ephemerisEngine = &engine;
    input.preparedProjection = &projection.value();
    input.skyContext.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
    input.renderTheme = makeTrailRenderTheme();
    input.viewportWidth = 1000.0;
    input.viewportHeight = 800.0;

    QElapsedTimer timer;
    timer.start();
    for (std::uint32_t bodyIndex = 0; bodyIndex < 120U; ++bodyIndex) {
        input.targetBodyIndex = bodyIndex;
        builder.appendTrail(frame, input);
    }
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(!frame.lines.empty());
    verifyElapsedBelow(elapsedMs, kManyTrailsBudgetMs, "many object trails");
}

void PerformanceGuardTests::packagedResourceDependenciesResolve()
{
    const skygate::ui::internal::SkyThemeRepository themeRepository;
    QVERIFY(themeRepository.themeOptions().size() >= 2);
    QVERIFY(themeRepository.defaultTheme().ui.windowBackground.isValid());

    LocationCatalogModel locations;
    QVERIFY(locations.rowCount() > 0);
    QVERIFY(!locations.index(0, 0).data(LocationCatalogModel::DisplayTextRole).toString().isEmpty());
}

void PerformanceGuardTests::platformNativeHooksTolerateEmptyInputs()
{
#ifdef Q_OS_MACOS
    skygate::ui::setMacDockIcon(QString());
#else
    QSKIP("Mac dock icon hook is only built on macOS.");
#endif
}

QTEST_GUILESS_MAIN(PerformanceGuardTests)

#include "PerformanceGuardTests.moc"
