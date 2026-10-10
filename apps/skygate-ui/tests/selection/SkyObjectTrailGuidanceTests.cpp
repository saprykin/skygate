#include "CelestialBodyCatalog.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "TrailBuilderTestEngine.hpp"
#include "TrailBuilderTestScenario.hpp"
#include "TrailRenderQuery.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "engine/SimpleEphemerisGuidanceStrategy.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "math/ViewportMath.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using skygate::ui::tests::TrailBuilderTestEngine;
using skygate::ui::tests::TrailBuilderTestScenario;
using skygate::ui::tests::TrailRenderQuery;

namespace {

class CurvedHighPrecisionTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Curved high-precision trail test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineTraits::highPrecisionEngine();
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        (void)bodyId;
        return computeBodyState(request, std::size_t{7U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(request.context.utcTime.time_since_epoch()).count()
        );
        m_requestOffsetMinutes.push_back(offsetMinutes);
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {
                .altitudeDeg = 12.0 + (8.0 * std::sin(static_cast<double>(offsetMinutes) * std::numbers::pi / 120.0)),
                .azimuthDeg = 180.0 + (0.08 * static_cast<double>(offsetMinutes))
            }
        };
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
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] const std::vector<int>& requestOffsetMinutes() const noexcept
    {
        return m_requestOffsetMinutes;
    }

private:
    mutable std::vector<int> m_requestOffsetMinutes;
};

class MarkerTrailGuidanceEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit MarkerTrailGuidanceEngine(std::shared_ptr<int> sampleCount) : m_sampleCount(std::move(sampleCount)) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Marker trail guidance test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++*m_sampleCount;
        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(request.context.utcTime.time_since_epoch()).count()
        );
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {
                .altitudeDeg = 45.0 + (static_cast<double>(offsetMinutes) / 6000.0),
                .azimuthDeg = 180.0 + (static_cast<double>(offsetMinutes) / 6000.0)
            }
        };
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot compute(const skygate::core::ObservationContext&) const override
    {
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, const std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        return std::nullopt;
    }

private:
    std::shared_ptr<int> m_sampleCount;
};

class MarkerTrailGuidanceStrategy final : public skygate::ephemeris::IEphemerisGuidanceStrategy {
public:
    [[nodiscard]] std::unique_ptr<skygate::ephemeris::IEphemerisEngine>
    createGuidanceEngine(const skygate::ephemeris::CelestialBodyCatalog&) const override
    {
        ++createCount;
        return std::make_unique<MarkerTrailGuidanceEngine>(m_sampleCount);
    }

    mutable int createCount = 0;
    std::shared_ptr<int> m_sampleCount = std::make_shared<int>(0);
};

skygate::ephemeris::EphemerisEngineTraits adaptiveTrailWithoutGuidanceApprovalTraits()
{
    skygate::ephemeris::EphemerisEngineTraits traits = skygate::ephemeris::EphemerisEngineTraits::noTraits();
    traits.prefersAdaptiveTrailSampling = true;
    return traits;
}

bool containsOffset(const std::vector<int>& offsets, const int offsetMinutes)
{
    return std::find(offsets.begin(), offsets.end(), offsetMinutes) != offsets.end();
}

}  // namespace

class SkyObjectTrailGuidanceTests final : public QObject {
    Q_OBJECT

private slots:
    void highPrecisionFixedTargetTrailSamplesThroughEngine();
    void highPrecisionRequestTrailUsesSparseInterpolatedSamples();
    void thirdIdentityHighPrecisionTraitsTrailUsesSparseInterpolatedSamples();
    void highPrecisionNonFixedTargetTrailUsesGuidanceEngine();
    void adaptiveTrailWithoutGuidanceApprovalUsesDirectSamplingForNonFixedTarget();
    void adaptiveTrailWithoutGuidanceApprovalSkipsFixedEquatorialShortcut();
    void guidanceStrategyInjectionIsHonored();
    void highPrecisionTrailRefinesCurvedInterpolation();
};

void SkyObjectTrailGuidanceTests::highPrecisionFixedTargetTrailSamplesThroughEngine()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setHighPrecisionTraits(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);
    input.ephemerisRequest = request;

    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "fixed-star";
    body.displayName = "Fixed Star";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.75, .declinationDeg = 2.0};
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{std::move(body)}
    );
    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.75, .declinationDeg = 2.0},
        .horizontal = skygate::core::HorizontalCoordinate{.altitudeDeg = 45.0, .azimuthDeg = 180.0}
    };
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;

    builder.appendTrail(frame, input);

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QCOMPARE(engine.requestBodyStateCalls(), 145);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), -350));
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), 10));
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), 1090));
}

void SkyObjectTrailGuidanceTests::highPrecisionRequestTrailUsesSparseInterpolatedSamples()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setHighPrecisionTraits(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);
    input.ephemerisRequest = request;

    builder.appendTrail(frame, input);

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QCOMPARE(TrailRenderQuery::countLabelsOfKind(frame.labels, "trailTick"), 3);
    QCOMPARE(engine.requestBodyStateCalls(), 25);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(engine.lastRequest().has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(engine.lastRequest()->options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(engine.lastRequest()->options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::astrometric())
    );
    QVERIFY(!engine.lastRequest()->options.enableAtmosphericRefraction());
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), -350));
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), -290));
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), 1090));

    const auto pannedProjection = TrailBuilderTestScenario::makeProjection(70.0);
    QVERIFY(pannedProjection.has_value());
    SkyRenderFrame pannedFrame;
    input.preparedProjection = &*pannedProjection;
    builder.appendTrail(pannedFrame, input);

    QVERIFY(pannedFrame.lines.size() > 40U);
    QCOMPARE(engine.requestBodyStateCalls(), 25);
}

void SkyObjectTrailGuidanceTests::thirdIdentityHighPrecisionTraitsTrailUsesSparseInterpolatedSamples()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setHighPrecisionTraits(true);
    engine.setNonEnumeratedKind(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(engine.kind());
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);
    input.ephemerisRequest = request;

    builder.appendTrail(frame, input);

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QCOMPARE(TrailRenderQuery::countLabelsOfKind(frame.labels, "trailTick"), 3);
    QCOMPARE(engine.requestBodyStateCalls(), 25);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
}

void SkyObjectTrailGuidanceTests::highPrecisionNonFixedTargetTrailUsesGuidanceEngine()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setHighPrecisionTraits(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    input.ephemerisRequest = request;

    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "moon";
    body.displayName = "Moon";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Moon;
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{std::move(body)}
    );

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> guidanceEngine =
        std::move(skygate::ephemeris::EphemerisEngineFactory::create(bodyCatalog).engine);
    QVERIFY(guidanceEngine != nullptr);
    skygate::ephemeris::EphemerisRequest guidanceRequest = request;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.setEngineKind(guidanceEngine->kind());
    const std::optional<skygate::ephemeris::CelestialBodyState> guidanceState =
        guidanceEngine->computeBodyState(guidanceRequest, std::size_t{0U});
    QVERIFY(guidanceState.has_value());
    QVERIFY(guidanceState->horizontal.isFinite());

    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 12.0},
        .horizontal = skygate::core::HorizontalCoordinate{
            .altitudeDeg = guidanceState->horizontal.altitudeDeg + 8.0,
            .azimuthDeg = guidanceState->horizontal.azimuthDeg + 8.0,
        }
    };
    const std::optional<skygate::core::PreparedProjection> anchoredProjection =
        skygate::core::PreparedProjection::create(
            skygate::core::ProjectionType::Stereographic,
            skygate::core::ViewportMath::buildProjectionParams(
                1000.0, 800.0, state.horizontal.altitudeDeg, state.horizontal.azimuthDeg, 90.0
            )
        );
    QVERIFY(anchoredProjection.has_value());
    input.preparedProjection = &*anchoredProjection;
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;
    input.guidanceStrategy = std::make_shared<skygate::ephemeris::SimpleEphemerisGuidanceStrategy>();

    builder.appendTrail(frame, input);

    QVERIFY(!frame.lines.empty());
    QCOMPARE(engine.requestBodyStateCalls(), 0);
    QCOMPARE(engine.contextBodyStateCalls(), 0);

    const skygate::core::ScreenPoint selectedPoint = anchoredProjection->project(state.horizontal);
    QVERIFY(selectedPoint.isVisible);
    QVERIFY(std::any_of(frame.lines.begin(), frame.lines.end(), [&](const SkyRenderLine& line) {
        return line.widthPx == 2.0 && line.color.alpha() == 175
               && TrailRenderQuery::lineTouchesPoint(line, selectedPoint.x, selectedPoint.y, 1.0);
    }));
    QVERIFY(std::none_of(frame.lines.begin(), frame.lines.end(), [&](const SkyRenderLine& line) {
        return TrailRenderQuery::lineTouchesPoint(line, selectedPoint.x, selectedPoint.y, 1.0)
               && TrailRenderQuery::lineLength(line) > 50.0;
    }));
}

void SkyObjectTrailGuidanceTests::adaptiveTrailWithoutGuidanceApprovalUsesDirectSamplingForNonFixedTarget()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setTraits(adaptiveTrailWithoutGuidanceApprovalTraits());
    engine.setNonEnumeratedKind(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(engine.kind());
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);
    input.ephemerisRequest = request;

    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "moon";
    body.displayName = "Moon";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Moon;
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{std::move(body)}
    );
    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 12.0},
        .horizontal = skygate::core::HorizontalCoordinate{.altitudeDeg = 45.0, .azimuthDeg = 180.0}
    };
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;

    const auto strategy = std::make_shared<MarkerTrailGuidanceStrategy>();
    input.guidanceStrategy = strategy;

    builder.appendTrail(frame, input);

    QCOMPARE(strategy->createCount, 0);
    QCOMPARE(engine.requestBodyStateCalls(), 25);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(!frame.lines.empty());
}

void SkyObjectTrailGuidanceTests::adaptiveTrailWithoutGuidanceApprovalSkipsFixedEquatorialShortcut()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setTraits(adaptiveTrailWithoutGuidanceApprovalTraits());
    engine.setNonEnumeratedKind(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(engine.kind());
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);
    input.ephemerisRequest = request;

    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "fixed-star";
    body.displayName = "Fixed Star";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.75, .declinationDeg = 2.0};
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{std::move(body)}
    );
    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.75, .declinationDeg = 2.0}
    };
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;

    builder.appendTrail(frame, input);

    QVERIFY(engine.requestBodyStateCalls() > 0);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(!frame.lines.empty());
}

void SkyObjectTrailGuidanceTests::guidanceStrategyInjectionIsHonored()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    engine.setHighPrecisionTraits(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    input.ephemerisRequest = request;

    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "moon";
    body.displayName = "Moon";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Moon;
    const skygate::ephemeris::CelestialBodyCatalog bodyCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{std::move(body)}
    );
    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 12.0},
        .horizontal = skygate::core::HorizontalCoordinate{.altitudeDeg = 45.0, .azimuthDeg = 180.0}
    };
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;

    const auto strategy = std::make_shared<MarkerTrailGuidanceStrategy>();
    input.guidanceStrategy = strategy;

    builder.appendTrail(frame, input);

    QCOMPARE(strategy->createCount, 1);
    QVERIFY(*strategy->m_sampleCount > 0);
    QCOMPARE(engine.requestBodyStateCalls(), 0);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(!frame.lines.empty());
}

void SkyObjectTrailGuidanceTests::highPrecisionTrailRefinesCurvedInterpolation()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    CurvedHighPrecisionTrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Utc
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(
        skygate::ephemeris::EphemerisCorrectionFlags::astrometric()
        | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    );
    request.options.setEnableAtmosphericRefraction(true);
    input.ephemerisRequest = request;

    builder.appendTrail(frame, input);

    QVERIFY(!frame.lines.empty());
    QVERIFY(engine.requestOffsetMinutes().size() > 25U);
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), -300));
    QVERIFY(containsOffset(engine.requestOffsetMinutes(), -330));
}

QTEST_APPLESS_MAIN(SkyObjectTrailGuidanceTests)

#include "SkyObjectTrailGuidanceTests.moc"
