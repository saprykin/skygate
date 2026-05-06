#include "SkyObjectTrailBuilder.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <algorithm>
#include <chrono>
#include <optional>

namespace {

class TrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit TrailEngine(const std::uint32_t expectedBodyIndex = 7U)
        : m_expectedBodyIndex(expectedBodyIndex)
    {
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        (void) context;
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        m_sawExpectedBodyIndex = m_sawExpectedBodyIndex || bodyIndex == m_expectedBodyIndex;

        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(
                context.utcTime.time_since_epoch()
            ).count()
        );
        if (m_gapAtPresent && offsetMinutes == 0) {
            return std::nullopt;
        }

        return skygate::ephemeris::CelestialBodyState {
            .bodyIndex = bodyIndex,
            .horizontal = {
                .altitudeDeg = 45.0 + (static_cast<double>(offsetMinutes) / 6000.0),
                .azimuthDeg = 180.0 + (static_cast<double>(offsetMinutes) / 6000.0)
            }
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

private:
    std::uint32_t m_expectedBodyIndex = 0;
    bool m_gapAtPresent = false;
    mutable bool m_sawExpectedBodyIndex = false;
};

class CrossingTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        (void) context;
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        const auto offsetMinutes = static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(
                context.utcTime.time_since_epoch()
            ).count()
        );
        if (offsetMinutes == -30) {
            return skygate::ephemeris::CelestialBodyState {
                .bodyIndex = bodyIndex,
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 179.0}
            };
        }
        if (offsetMinutes == 0) {
            return skygate::ephemeris::CelestialBodyState {
                .bodyIndex = bodyIndex,
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 181.0}
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

SkyObjectTrailInput makeInput(
    const skygate::ephemeris::IEphemerisEngine& engine,
    const skygate::core::PreparedProjection& projection
)
{
    SkyObjectTrailInput input;
    input.ephemerisEngine = &engine;
    input.preparedProjection = &projection;
    input.skyContext.observer = {
        .latitudeDeg = 47.0,
        .longitudeDeg = 8.0,
        .elevationMeters = 400.0
    };
    input.renderTheme = makeRenderTheme();
    input.targetBodyIndex = 7U;
    input.viewportWidth = 1000.0;
    input.viewportHeight = 800.0;
    return input;
}

int countLabelsOfKind(const QVariantList& labels, const QString& kind)
{
    int count = 0;
    for (const QVariant& label : labels) {
        if (label.toMap().value("kind").toString() == kind) {
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
