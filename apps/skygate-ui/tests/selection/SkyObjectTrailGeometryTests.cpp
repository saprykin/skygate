#include "CelestialBodyCatalog.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "TrailBuilderTestEngine.hpp"
#include "TrailBuilderTestScenario.hpp"
#include "TrailRenderQuery.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <QColor>
#include <QtTest/QtTest>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using skygate::ui::tests::TrailBuilderTestEngine;
using skygate::ui::tests::TrailBuilderTestScenario;
using skygate::ui::tests::TrailRenderQuery;

namespace {

class CrossingTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Crossing trail test engine";
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
        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count()
        );
        if (offsetMinutes == -30) {
            return skygate::ephemeris::CelestialBodyState{
                .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 179.0}
            };
        }
        if (offsetMinutes == 0) {
            return skygate::ephemeris::CelestialBodyState{
                .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 181.0}
            };
        }

        return std::nullopt;
    }
};

[[nodiscard]] skygate::ephemeris::EphemerisRequest makeTrailRequest(const SkyObjectTrailInput& input)
{
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::core::TimeScale::Utc
    };
    return request;
}

}  // namespace

class SkyObjectTrailGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidInputsAppendNoLines();
    void appendsPastDashesFutureSegmentsAndTickLabels();
    void invalidSamplesBreakContinuity();
    void highPrecisionFixedTrailAlignsPresentToMarker();
    void longProjectedJumpsAreDropped();
    void offscreenTrailSamplesStillRenderCrossingSegment();
    void identicalRequestsReuseTrailSamples();
    void viewportAndThemeChangesReuseTrailSamples();
    void epochChangeResamplesTrail();
    void optionChangeResamplesTrail();
    void targetChangeResamplesTrail();
    void engineChangeResamplesTrail();
    void observerChangeResamplesTrail();
};

void SkyObjectTrailGeometryTests::invalidInputsAppendNoLines()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    const TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);

    input.ephemerisEngine = nullptr;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());

    input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.preparedProjection = nullptr;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());

    input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.skyContext.observer.latitudeDeg = 100.0;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());
}

void SkyObjectTrailGeometryTests::appendsPastDashesFutureSegmentsAndTickLabels()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;

    builder.appendTrail(frame, TrailBuilderTestScenario::makeInput(engine, *projection));

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QCOMPARE(TrailRenderQuery::countLabelsOfKind(frame.labels, "trailTick"), 3);
    QVERIFY(std::any_of(frame.lines.begin(), frame.lines.end(), [](const SkyRenderLine& line) {
        return line.widthPx == 1.4 && line.color.alpha() == 105;
    }));
    QVERIFY(std::any_of(frame.lines.begin(), frame.lines.end(), [](const SkyRenderLine& line) {
        return line.widthPx == 2.0 && line.color.alpha() == 175;
    }));
}

void SkyObjectTrailGeometryTests::invalidSamplesBreakContinuity()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine continuousEngine;
    TrailBuilderTestEngine gappedEngine;
    gappedEngine.setGapAtPresent(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame continuousFrame;
    SkyRenderFrame gappedFrame;

    builder.appendTrail(continuousFrame, TrailBuilderTestScenario::makeInput(continuousEngine, *projection));
    builder.appendTrail(gappedFrame, TrailBuilderTestScenario::makeInput(gappedEngine, *projection));

    QVERIFY(!continuousFrame.lines.empty());
    QVERIFY(gappedFrame.lines.size() < continuousFrame.lines.size());
}

void SkyObjectTrailGeometryTests::highPrecisionFixedTrailAlignsPresentToMarker()
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
        .horizontal = skygate::core::HorizontalCoordinate{.altitudeDeg = 44.0, .azimuthDeg = 182.0}
    };
    input.targetBody = &bodyCatalog.bodyAt(0U);
    input.targetState = &state;

    builder.appendTrail(frame, input);

    const skygate::core::ScreenPoint selectedPoint = projection->project(state.horizontal);
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

void SkyObjectTrailGeometryTests::longProjectedJumpsAreDropped()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.viewportWidth = 1.0;
    input.viewportHeight = 1.0;

    builder.appendTrail(frame, input);

    QVERIFY(std::none_of(frame.lines.begin(), frame.lines.end(), [](const SkyRenderLine& line) {
        if (line.widthPx != 2.0 || line.x1 == line.x2 || line.y1 == line.y2) {
            return false;
        }
        const double dx = line.x2 - line.x1;
        const double dy = line.y2 - line.y1;
        return (dx * dx + dy * dy) > (0.35 * 0.35);
    }));
}

void SkyObjectTrailGeometryTests::offscreenTrailSamplesStillRenderCrossingSegment()
{
    const auto projection = TrailBuilderTestScenario::makeProjection(1.0);
    QVERIFY(projection.has_value());
    const CrossingTrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;

    builder.appendTrail(frame, TrailBuilderTestScenario::makeInput(engine, *projection));

    QVERIFY(!frame.lines.empty());
}

void SkyObjectTrailGeometryTests::identicalRequestsReuseTrailSamples()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);

    builder.appendTrail(frame, input);
    const int sampleCalls = engine.requestBodyStateCalls();
    QVERIFY(sampleCalls > 0);

    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QCOMPARE(engine.requestBodyStateCalls(), sampleCalls);
    QCOMPARE(secondFrame.lines.size(), frame.lines.size());
}

void SkyObjectTrailGeometryTests::viewportAndThemeChangesReuseTrailSamples()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);
    SkyRenderFrame frame;

    builder.appendTrail(frame, input);
    const int sampleCalls = engine.requestBodyStateCalls();
    QVERIFY(sampleCalls > 0);

    const auto widerProjection = TrailBuilderTestScenario::makeProjection(70.0);
    QVERIFY(widerProjection.has_value());
    input.preparedProjection = &*widerProjection;
    input.viewportWidth = 1600.0;
    input.viewportHeight = 1200.0;
    input.renderTheme.selectionMarkerBorder = QColor("#ff4040");
    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QCOMPARE(engine.requestBodyStateCalls(), sampleCalls);
    QVERIFY(!secondFrame.lines.empty());
}

void SkyObjectTrailGeometryTests::epochChangeResamplesTrail()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);

    builder.appendTrail(frame, input);
    const int sampleCalls = engine.requestBodyStateCalls();
    QVERIFY(sampleCalls > 0);

    input.ephemerisRequest->epoch.julianDatePart2 += 1.0 / 86'400'000.0;
    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QVERIFY(engine.requestBodyStateCalls() > sampleCalls);
}

void SkyObjectTrailGeometryTests::optionChangeResamplesTrail()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);

    builder.appendTrail(frame, input);
    const int sampleCalls = engine.requestBodyStateCalls();
    QVERIFY(sampleCalls > 0);

    input.ephemerisRequest->options.setAtmosphericPressureHpa(900.0);
    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QVERIFY(engine.requestBodyStateCalls() > sampleCalls);
}

void SkyObjectTrailGeometryTests::targetChangeResamplesTrail()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);
    SkyRenderFrame frame;

    builder.appendTrail(frame, input);
    const int sampleCalls = engine.requestBodyStateCalls();
    QVERIFY(sampleCalls > 0);

    input.targetBodyIndex = 9U;
    SkyRenderFrame indexFrame;
    builder.appendTrail(indexFrame, input);
    const int indexCalls = engine.requestBodyStateCalls();
    QVERIFY(indexCalls > sampleCalls);

    const skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = input.targetBodyIndex,
        .equatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 12.0}
    };
    input.targetState = &state;
    SkyRenderFrame stateFrame;
    builder.appendTrail(stateFrame, input);

    QVERIFY(engine.requestBodyStateCalls() > indexCalls);
}

void SkyObjectTrailGeometryTests::engineChangeResamplesTrail()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine firstEngine;
    TrailBuilderTestEngine secondEngine;
    const SkyObjectTrailBuilder builder;
    auto input = TrailBuilderTestScenario::makeInput(firstEngine, *projection);
    input.ephemerisRequest = makeTrailRequest(input);
    SkyRenderFrame frame;

    builder.appendTrail(frame, input);
    const int firstCalls = firstEngine.requestBodyStateCalls();
    QVERIFY(firstCalls > 0);

    input.ephemerisEngine = &secondEngine;
    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QCOMPARE(firstEngine.requestBodyStateCalls(), firstCalls);
    QVERIFY(secondEngine.requestBodyStateCalls() > 0);
}

void SkyObjectTrailGeometryTests::observerChangeResamplesTrail()
{
    const auto projection = TrailBuilderTestScenario::makeProjection();
    QVERIFY(projection.has_value());
    TrailBuilderTestEngine engine;
    const SkyObjectTrailBuilder builder;
    auto input = TrailBuilderTestScenario::makeInput(engine, *projection);
    SkyRenderFrame frame;

    builder.appendTrail(frame, input);
    const int contextCalls = engine.contextBodyStateCalls();
    QVERIFY(contextCalls > 0);

    input.skyContext.observer.longitudeDeg = 9.0;
    SkyRenderFrame secondFrame;
    builder.appendTrail(secondFrame, input);

    QVERIFY(engine.contextBodyStateCalls() > contextCalls);
}

QTEST_APPLESS_MAIN(SkyObjectTrailGeometryTests)

#include "SkyObjectTrailGeometryTests.moc"
