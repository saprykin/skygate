#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyHitTargetIndex.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "SkySceneModel.hpp"

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IApparentPlaceCalculator.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/highprecision/IStarAstrometryCalculator.hpp"
#include "skygate/ephemeris/TimeScaleService.hpp"

#include "skygate/testsupport/PerformanceBudget.hpp"

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
    void profilesHighPrecisionLargeFixedCatalogSelection();
    void profilesHighPrecisionMoonSearchSelection();
    void searchesLargeMixedCatalogWithinGuardrail();
    void hitTestsDenseRenderFrameWithinGuardrail();
    void buildsManyObjectTrailsWithinGuardrail();
    void wallClockBudgetsAreAdvisoryByDefault();
};

namespace {

constexpr qint64 kLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kHighPrecisionLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kHighPrecisionCacheKeyBudgetMs = 2000;
constexpr qint64 kLargeSearchLoadBudgetMs = 10000;
constexpr qint64 kLargeSearchQueryBudgetMs = 3000;
constexpr qint64 kDenseHitTestBudgetMs = 5000;
constexpr qint64 kManyTrailsBudgetMs = 12000;

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

class GuardSolarSystemStateCalculator final : public skygate::ephemeris::highprecision::ISolarSystemStateCalculator {
public:
    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult
    calculate(const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        const double offsetMinutes = static_cast<double>(
            std::chrono::duration_cast<std::chrono::minutes>(input.request.context.utcTime.time_since_epoch()).count()
        );
        skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result;
        result.equatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = std::fmod(5.0 + (offsetMinutes / 6000.0), 24.0),
            .declinationDeg = 18.0 + std::sin(offsetMinutes / 800.0),
        };
        result.horizontal = skygate::core::HorizontalCoordinate{
            .altitudeDeg = 38.0 + std::sin(offsetMinutes / 180.0) * 12.0,
            .azimuthDeg = std::fmod(140.0 + (offsetMinutes / 4.0), 360.0),
        };
        result.metadata.status = skygate::ephemeris::EphemerisResultStatus::Valid;
        result.metadata.dataSourceProvenance = "performance guard solar system";
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class GuardTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convert(
        const skygate::ephemeris::AstronomicalEpoch& epoch, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        skygate::ephemeris::TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::ephemeris::CivilDateTime& dateTime, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        const std::optional<skygate::ephemeris::AstronomicalEpoch> epoch =
            skygate::ephemeris::astronomicalEpochFromCivilDateTime(dateTime);
        if (epoch.has_value()) {
            return convert(*epoch, targetScale);
        }

        skygate::ephemeris::TimeScaleConversionResult result;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Failed;
        result.addWarning(skygate::ephemeris::TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }
};

void verifyElapsedBelow(const qint64 elapsedMs, const qint64 budgetMs, const char* operationName)
{
    const skygate::testsupport::PerformanceBudgetDecision decision =
        skygate::testsupport::reportPerformanceBudget(elapsedMs, budgetMs, operationName);
    QVERIFY2(
        decision != skygate::testsupport::PerformanceBudgetDecision::StrictOverBudget,
        qPrintable(
            skygate::testsupport::strictPerformanceBudgetHint(
                skygate::testsupport::performanceMetricMessage(elapsedMs, budgetMs, operationName, true)
            )
        )
    );
}

void verifyCounterDelta(
    const int before, const int after, const int expectedDelta, const char* counterName, const char* operationName
)
{
    const int actualDelta = after - before;
    const QString message = QStringLiteral("perf counter %1 during %2: expected delta=%3 actual delta=%4")
                                .arg(QString::fromUtf8(counterName))
                                .arg(QString::fromUtf8(operationName))
                                .arg(expectedDelta)
                                .arg(actualDelta);
    QVERIFY2(actualDelta == expectedDelta, qPrintable(message));
}

void verifyHighPrecisionCounterDeltas(
    const GuardBatchStarAstrometryCalculator& starAstrometryCalculator,
    const GuardApparentPlaceCalculator& apparentPlaceCalculator,
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
    verifyCounterDelta(
        astrometryBatchCallsBefore,
        starAstrometryCalculator.batchCallCount(),
        expectedAstrometryBatchDelta,
        "astrometry batch",
        operationName
    );
    verifyCounterDelta(
        astrometrySingleCallsBefore,
        starAstrometryCalculator.singleCallCount(),
        expectedAstrometrySingleDelta,
        "astrometry single",
        operationName
    );
    verifyCounterDelta(
        apparentBatchCallsBefore,
        apparentPlaceCalculator.batchCallCount(),
        expectedApparentBatchDelta,
        "apparent batch",
        operationName
    );
    verifyCounterDelta(
        apparentSingleCallsBefore,
        apparentPlaceCalculator.singleCallCount(),
        expectedApparentSingleDelta,
        "apparent single",
        operationName
    );
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

skygate::ephemeris::CelestialBody makeHighPrecisionMoonBody()
{
    skygate::ephemeris::CelestialBody body;
    body.id = "moon";
    body.displayName = "Moon";
    body.type = skygate::ephemeris::CelestialBodyType::Moon;
    body.visualMagnitude = -12.0;
    body.ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Moon;
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

std::vector<skygate::ephemeris::CelestialBody> makeHighPrecisionMoonGuardCatalog()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeHighPrecisionGuardCatalog();
    bodies.insert(bodies.begin(), makeHighPrecisionMoonBody());
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
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(makeLargeMixedCatalog());
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
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
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

void PerformanceGuardTests::profilesHighPrecisionLargeFixedCatalogSelection()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeHighPrecisionGuardCatalog();
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
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
        kHighPrecisionLargeSceneBuildBudgetMs,
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
    verifyHighPrecisionCounterDeltas(
        *starAstrometryCalculator,
        *apparentPlaceCalculator,
        astrometryBatchCallsBefore,
        astrometrySingleCallsBefore,
        apparentBatchCallsBefore,
        apparentSingleCallsBefore,
        0,
        1,
        0,
        1,
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
        kHighPrecisionLargeSceneBuildBudgetMs,
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
        kHighPrecisionLargeSceneBuildBudgetMs,
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
        kHighPrecisionLargeSceneBuildBudgetMs,
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

void PerformanceGuardTests::profilesHighPrecisionMoonSearchSelection()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeHighPrecisionMoonGuardCatalog();
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    QVERIFY(catalog != nullptr);

    auto solarSystemCalculator = std::make_shared<GuardSolarSystemStateCalculator>();
    auto starAstrometryCalculator = std::make_shared<GuardBatchStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<GuardApparentPlaceCalculator>();
    auto computationCache = std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();
    auto timeScaleService = std::make_shared<GuardTimeScaleService>();

    skygate::ephemeris::EphemerisEngineOptions options;
    options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Geometric;

    skygate::ephemeris::highprecision::HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = solarSystemCalculator;
    dependencies.starAstrometryCalculator = starAstrometryCalculator;
    dependencies.timeScaleService = timeScaleService;
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
    sceneModel.setViewportSize(1280.0, 800.0);
    QVERIFY(sceneModel.snapshotGeneration() > 0U);
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);

    const int solarCallsBefore = solarSystemCalculator->callCount();
    const int astrometryBatchCallsBefore = starAstrometryCalculator->batchCallCount();
    const int astrometrySingleCallsBefore = starAstrometryCalculator->singleCallCount();
    const int apparentBatchCallsBefore = apparentPlaceCalculator->batchCallCount();
    const int apparentSingleCallsBefore = apparentPlaceCalculator->singleCallCount();

    QElapsedTimer timer;
    timer.start();
    QVERIFY(controller.focusSearchTarget(QStringLiteral("body"), QStringLiteral("moon")));
    const qint64 elapsedMs = timer.elapsed();

    qInfo().noquote() << skygate::testsupport::performanceMetricMessage(
        elapsedMs,
        kHighPrecisionLargeSceneBuildBudgetMs,
        "high precision moon search selection",
        skygate::testsupport::isStrictPerformanceGuardMode()
    );
    qInfo().noquote() << QStringLiteral(
                             "perf high precision moon search counters: solarDelta=%1 astrometryBatchDelta=%2 "
                             "astrometrySingleDelta=%3 apparentBatchDelta=%4 apparentSingleDelta=%5"
    )
                             .arg(solarSystemCalculator->callCount() - solarCallsBefore)
                             .arg(starAstrometryCalculator->batchCallCount() - astrometryBatchCallsBefore)
                             .arg(starAstrometryCalculator->singleCallCount() - astrometrySingleCallsBefore)
                             .arg(apparentPlaceCalculator->batchCallCount() - apparentBatchCallsBefore)
                             .arg(apparentPlaceCalculator->singleCallCount() - apparentSingleCallsBefore);

    QCOMPARE(controller.selectedSearchTargetKind(), QStringLiteral("body"));
    QCOMPARE(controller.selectedSearchTargetId(), QStringLiteral("moon"));
    verifyCounterDelta(
        astrometryBatchCallsBefore,
        starAstrometryCalculator->batchCallCount(),
        0,
        "astrometry batch",
        "high precision moon search selection"
    );
    verifyCounterDelta(
        astrometrySingleCallsBefore,
        starAstrometryCalculator->singleCallCount(),
        0,
        "astrometry single",
        "high precision moon search selection"
    );
    verifyCounterDelta(
        apparentBatchCallsBefore,
        apparentPlaceCalculator->batchCallCount(),
        0,
        "apparent batch",
        "high precision moon search selection"
    );
    verifyCounterDelta(
        apparentSingleCallsBefore,
        apparentPlaceCalculator->singleCallCount(),
        0,
        "apparent single",
        "high precision moon search selection"
    );
    verifyElapsedBelow(elapsedMs, kHighPrecisionLargeSceneBuildBudgetMs, "high precision moon search selection");
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

void PerformanceGuardTests::wallClockBudgetsAreAdvisoryByDefault()
{
    using skygate::testsupport::PerformanceBudgetDecision;

    QCOMPARE(
        skygate::testsupport::performanceBudgetDecision(200, 100, false), PerformanceBudgetDecision::AdvisoryOverBudget
    );
    QCOMPARE(
        skygate::testsupport::performanceBudgetDecision(200, 100, true), PerformanceBudgetDecision::StrictOverBudget
    );
    QCOMPARE(skygate::testsupport::performanceBudgetDecision(99, 100, true), PerformanceBudgetDecision::WithinBudget);
}

QTEST_GUILESS_MAIN(PerformanceGuardTests)

#include "PerformanceGuardTests.moc"
