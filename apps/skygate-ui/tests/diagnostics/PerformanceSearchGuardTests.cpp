#include "CelestialBodyCatalog.hpp"
#include "GuardApparentPlaceCalculator.hpp"
#include "GuardBatchStarAstrometryCalculator.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PerformanceGuardChecks.hpp"
#include "PerformanceGuardScenario.hpp"
#include "SkyContextController.hpp"
#include "SkyHitTargetIndex.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "SkySceneModel.hpp"
#include "catalog/CatalogFactory.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/ITimeScaleService.hpp"
#include "math/ViewportMath.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "skygate/testsupport/PerformanceBudget.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QtTest>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using skygate::ui::tests::GuardApparentPlaceCalculator;
using skygate::ui::tests::GuardBatchStarAstrometryCalculator;
using skygate::ui::tests::PerformanceGuardChecks;
using skygate::ui::tests::PerformanceGuardScenario;

class PerformanceSearchGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void profilesHighPrecisionMoonSearchSelection();
    void searchesLargeMixedCatalogWithinGuardrail();
    void hitTestsDenseRenderFrameWithinGuardrail();
    void buildsManyObjectTrailsWithinGuardrail();
    void wallClockBudgetsAreAdvisoryByDefault();
};

namespace {

constexpr qint64 kLargeSearchLoadBudgetMs = 10000;
constexpr qint64 kLargeSearchQueryBudgetMs = 3000;
constexpr qint64 kDenseHitTestBudgetMs = 5000;
constexpr qint64 kManyTrailsBudgetMs = 12000;

class PerformanceTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Performance trail test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
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
        return computeBodyState(request.context, bodyIndex);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        (void)context;
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::size_t bodyIndex) const override
    {
        const double offsetMinutes = static_cast<double>(
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count()
        );
        const double bodyOffset = static_cast<double>(bodyIndex % 40U) * 0.015;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {
                .altitudeDeg = 45.0 + bodyOffset + (offsetMinutes / 8000.0),
                .azimuthDeg = 180.0 + bodyOffset + (offsetMinutes / 8000.0)
            }
        };
    }
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
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
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
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale) const override
    {
        skygate::ephemeris::TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
    ) const override
    {
        const std::optional<skygate::core::AstronomicalEpoch> epoch =
            skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        if (epoch.has_value()) {
            return convert(*epoch, targetScale);
        }

        skygate::ephemeris::TimeScaleConversionResult result;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Failed;
        result.addWarning(skygate::ephemeris::TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }
};

skygate::ephemeris::EphemerisSnapshot makeHitTestSnapshot(const int bodyCount)
{
    skygate::ephemeris::EphemerisSnapshot snapshot;
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies;
    bodies.reserve(static_cast<std::size_t>(bodyCount));
    for (int index = 0; index < bodyCount; ++index) {
        bodies.push_back(
            PerformanceGuardScenario::makeBody(
                "hit_body_" + std::to_string(index),
                "Hit Body " + std::to_string(index),
                skygate::ephemeris::BaseCelestialBody::Kind::Star,
                5.0,
                0.0,
                0.0
            )
        );
    }
    snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(std::move(bodies));
    return snapshot;
}

skygate::ui::internal::SkyThemeRenderPalette makeTrailRenderTheme()
{
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    renderTheme.selectionMarkerBorder = QColor("#80c0ff");
    return renderTheme;
}

}  // namespace

void PerformanceSearchGuardTests::profilesHighPrecisionMoonSearchSelection()
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies =
        PerformanceGuardScenario::makeHighPrecisionMoonGuardCatalog();
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    QVERIFY(catalog != nullptr);
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(bodies);

    auto solarSystemCalculator = std::make_shared<GuardSolarSystemStateCalculator>();
    auto starAstrometryCalculator = std::make_shared<GuardBatchStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<GuardApparentPlaceCalculator>();
    auto computationCache = std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();
    auto timeScaleService = std::make_shared<GuardTimeScaleService>();

    skygate::ephemeris::EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());

    skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.solarSystemStateCalculator = solarSystemCalculator;
    dependencies.starAstrometryCalculator = starAstrometryCalculator;
    dependencies.timeScaleService = timeScaleService;
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
        PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs,
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
    PerformanceGuardChecks::verifyCounterDelta(
        astrometryBatchCallsBefore,
        starAstrometryCalculator->batchCallCount(),
        0,
        "astrometry batch",
        "high precision moon search selection"
    );
    PerformanceGuardChecks::verifyCounterDelta(
        astrometrySingleCallsBefore,
        starAstrometryCalculator->singleCallCount(),
        0,
        "astrometry single",
        "high precision moon search selection"
    );
    PerformanceGuardChecks::verifyCounterDelta(
        apparentBatchCallsBefore,
        apparentPlaceCalculator->batchCallCount(),
        0,
        "apparent batch",
        "high precision moon search selection"
    );
    PerformanceGuardChecks::verifyCounterDelta(
        apparentSingleCallsBefore,
        apparentPlaceCalculator->singleCallCount(),
        0,
        "apparent single",
        "high precision moon search selection"
    );
    PerformanceGuardChecks::verifyElapsedBelow(
        elapsedMs, PerformanceGuardChecks::kHighPrecisionLargeSceneBuildBudgetMs, "high precision moon search selection"
    );
}

void PerformanceSearchGuardTests::searchesLargeMixedCatalogWithinGuardrail()
{
    skygate::ephemeris::CelestialBodyCatalog bodyCatalog = PerformanceGuardScenario::makeLargeMixedCatalog(true);

    SkyObjectSearchModel model;
    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups{
        {"Guard Constellation", {"guard_star_1", "guard_star_2"}}
    };
    QElapsedTimer timer;
    timer.start();
    model.setCatalogData(bodyCatalog.bodies(), anchorGroups);
    const qint64 loadElapsedMs = timer.elapsed();
    PerformanceGuardChecks::verifyElapsedBelow(loadElapsedMs, kLargeSearchLoadBudgetMs, "large search catalog load");

    timer.restart();
    model.setFilterText(QStringLiteral("guard exact target"));
    const qint64 exactQueryElapsedMs = timer.elapsed();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("guard_exact_target")
    );
    PerformanceGuardChecks::verifyElapsedBelow(exactQueryElapsedMs, kLargeSearchQueryBudgetMs, "large exact search");

    timer.restart();
    model.setFilterText(QStringLiteral("shared collision alias 7"));
    const qint64 aliasQueryElapsedMs = timer.elapsed();
    QVERIFY(model.rowCount() > 0);
    PerformanceGuardChecks::verifyElapsedBelow(aliasQueryElapsedMs, kLargeSearchQueryBudgetMs, "large alias search");
}

void PerformanceSearchGuardTests::hitTestsDenseRenderFrameWithinGuardrail()
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
    PerformanceGuardChecks::verifyElapsedBelow(elapsedMs, kDenseHitTestBudgetMs, "dense hit testing");
}

void PerformanceSearchGuardTests::buildsManyObjectTrailsWithinGuardrail()
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
    PerformanceGuardChecks::verifyElapsedBelow(elapsedMs, kManyTrailsBudgetMs, "many object trails");
}

void PerformanceSearchGuardTests::wallClockBudgetsAreAdvisoryByDefault()
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

QTEST_GUILESS_MAIN(PerformanceSearchGuardTests)

#include "PerformanceSearchGuardTests.moc"
