#include "SkyObjectTrailBuilder.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace {

class TrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit TrailEngine(const std::uint32_t expectedBodyIndex = 7U) : m_expectedBodyIndex(expectedBodyIndex) {}

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::ephemeris::EphemerisRequest& request
    ) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        (void)bodyId;
        return computeBodyState(request, std::size_t{m_expectedBodyIndex});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++m_requestBodyStateCalls;
        m_lastRequest = request;
        return stateForContext(request.context, static_cast<std::uint32_t>(bodyIndex));
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
        ++m_contextBodyStateCalls;
        return stateForContext(context, bodyIndex);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    stateForContext(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const
    {
        m_sawExpectedBodyIndex = m_sawExpectedBodyIndex || bodyIndex == m_expectedBodyIndex;

        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count()
        );
        if (m_gapAtPresent && offsetMinutes == 0) {
            return std::nullopt;
        }

        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = bodyIndex,
            .horizontal =
                {.altitudeDeg = 45.0 + (static_cast<double>(offsetMinutes) / 6000.0),
                 .azimuthDeg = 180.0 + (static_cast<double>(offsetMinutes) / 6000.0)}
        };
    }

    void setGapAtPresent(const bool gapAtPresent) noexcept
    {
        m_gapAtPresent = gapAtPresent;
    }

    [[nodiscard]] bool sawExpectedBodyIndex() const noexcept
    {
        return m_sawExpectedBodyIndex;
    }

    [[nodiscard]] int requestBodyStateCalls() const noexcept
    {
        return m_requestBodyStateCalls;
    }

    [[nodiscard]] int contextBodyStateCalls() const noexcept
    {
        return m_contextBodyStateCalls;
    }

    [[nodiscard]] const std::optional<skygate::ephemeris::EphemerisRequest>& lastRequest() const noexcept
    {
        return m_lastRequest;
    }

private:
    std::uint32_t m_expectedBodyIndex = 0;
    bool m_gapAtPresent = false;
    mutable bool m_sawExpectedBodyIndex = false;
    mutable int m_requestBodyStateCalls = 0;
    mutable int m_contextBodyStateCalls = 0;
    mutable std::optional<skygate::ephemeris::EphemerisRequest> m_lastRequest;
};

class CrossingTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::ephemeris::EphemerisRequest& request
    ) const override
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
        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count()
        );
        if (offsetMinutes == -30) {
            return skygate::ephemeris::CelestialBodyState{
                .bodyIndex = bodyIndex, .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 179.0}
            };
        }
        if (offsetMinutes == 0) {
            return skygate::ephemeris::CelestialBodyState{
                .bodyIndex = bodyIndex, .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 181.0}
            };
        }

        return std::nullopt;
    }
};

skygate::ui::internal::SkyThemeRenderPalette makeRenderTheme()
{
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    renderTheme.selectionMarkerBorder = QColor("#80c0ff");
    return renderTheme;
}

std::optional<skygate::core::PreparedProjection> makeProjection(const double fovDeg = 90.0)
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, fovDeg)
    );
}

SkyObjectTrailInput
makeInput(const skygate::ephemeris::IEphemerisEngine& engine, const skygate::core::PreparedProjection& projection)
{
    SkyObjectTrailInput input;
    input.ephemerisEngine = &engine;
    input.preparedProjection = &projection;
    input.skyContext.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
    input.renderTheme = makeRenderTheme();
    input.targetBodyIndex = 7U;
    input.viewportWidth = 1000.0;
    input.viewportHeight = 800.0;
    return input;
}

int countLabelsOfKind(const std::vector<SkyRenderLabel>& labels, const QString& kind)
{
    int count = 0;
    for (const SkyRenderLabel& label : labels) {
        if (label.kind == kind) {
            ++count;
        }
    }
    return count;
}

}  // namespace

class SkyObjectTrailBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidInputsAppendNoLines();
    void appendsPastDashesFutureSegmentsAndTickLabels();
    void invalidSamplesBreakContinuity();
    void requestTrailSamplingUsesSelectedEngineOptions();
    void longProjectedJumpsAreDropped();
    void offscreenTrailSamplesStillRenderCrossingSegment();
};

void SkyObjectTrailBuilderTests::invalidInputsAppendNoLines()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());
    const TrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = makeInput(engine, *projection);

    input.ephemerisEngine = nullptr;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());

    input = makeInput(engine, *projection);
    input.preparedProjection = nullptr;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());

    input = makeInput(engine, *projection);
    input.skyContext.observer.latitudeDeg = 100.0;
    builder.appendTrail(frame, input);
    QVERIFY(frame.lines.empty());
}

void SkyObjectTrailBuilderTests::appendsPastDashesFutureSegmentsAndTickLabels()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());
    TrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;

    builder.appendTrail(frame, makeInput(engine, *projection));

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QCOMPARE(countLabelsOfKind(frame.labels, "trailTick"), 3);
    QVERIFY(std::any_of(frame.lines.begin(), frame.lines.end(), [](const SkyRenderLine& line) {
        return line.widthPx == 1.4 && line.color.alpha() == 105;
    }));
    QVERIFY(std::any_of(frame.lines.begin(), frame.lines.end(), [](const SkyRenderLine& line) {
        return line.widthPx == 2.0 && line.color.alpha() == 175;
    }));
}

void SkyObjectTrailBuilderTests::invalidSamplesBreakContinuity()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());
    TrailEngine continuousEngine;
    TrailEngine gappedEngine;
    gappedEngine.setGapAtPresent(true);
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame continuousFrame;
    SkyRenderFrame gappedFrame;

    builder.appendTrail(continuousFrame, makeInput(continuousEngine, *projection));
    builder.appendTrail(gappedFrame, makeInput(gappedEngine, *projection));

    QVERIFY(!continuousFrame.lines.empty());
    QVERIFY(gappedFrame.lines.size() < continuousFrame.lines.size());
}

void SkyObjectTrailBuilderTests::requestTrailSamplingUsesSelectedEngineOptions()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());
    TrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = makeInput(engine, *projection);
    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::ephemeris::TimeScale::Utc
    };
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Astrometric;
    request.options.enableAtmosphericRefraction = false;
    input.ephemerisRequest = request;

    builder.appendTrail(frame, input);

    QVERIFY(engine.sawExpectedBodyIndex());
    QVERIFY(frame.lines.size() > 40U);
    QVERIFY(engine.requestBodyStateCalls() > 0);
    QCOMPARE(engine.contextBodyStateCalls(), 0);
    QVERIFY(engine.lastRequest().has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(engine.lastRequest()->options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(engine.lastRequest()->options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::Astrometric)
    );
    QVERIFY(!engine.lastRequest()->options.enableAtmosphericRefraction);
    QCOMPARE(engine.lastRequest()->context.utcTime.time_since_epoch().count(), std::int64_t{65400});
    QCOMPARE(engine.lastRequest()->epoch.julianDatePart1, 2'451'546.0);
    QVERIFY(std::abs(engine.lastRequest()->epoch.julianDatePart2) < 1e-12);
}

void SkyObjectTrailBuilderTests::longProjectedJumpsAreDropped()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());
    TrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    auto input = makeInput(engine, *projection);
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

void SkyObjectTrailBuilderTests::offscreenTrailSamplesStillRenderCrossingSegment()
{
    const auto projection = makeProjection(1.0);
    QVERIFY(projection.has_value());
    const CrossingTrailEngine engine;
    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;

    builder.appendTrail(frame, makeInput(engine, *projection));

    QVERIFY(!frame.lines.empty());
}

QTEST_APPLESS_MAIN(SkyObjectTrailBuilderTests)

#include "SkyObjectTrailBuilderTests.moc"
