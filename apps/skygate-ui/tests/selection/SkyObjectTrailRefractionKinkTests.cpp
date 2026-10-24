#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "math/ViewportMath.hpp"
#include "render/SkyRenderLine.hpp"
#include "selection/SkyObjectTrailBuilder.hpp"
#include "selection/SkyObjectTrailInput.hpp"
#include "support/TrailBuilderTestScenario.hpp"
#include "support/TrailRenderQuery.hpp"
#include "time/EpochCodec.hpp"

#include <QColor>
#include <QtTest/QtTest>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <ratio>
#include <string_view>
#include <vector>

namespace {

constexpr double kViewportWidthPx = 1000.0;
constexpr double kViewportHeightPx = 800.0;
constexpr double kFieldOfViewDeg = 60.0;
constexpr double kTrailStepMinutes = 10.0;
constexpr double kPresentAltitudeDeg = 0.55;
constexpr double kAltitudeStepPerTrailStepDeg = 0.3;
constexpr double kPresentAzimuthDeg = 180.0;
constexpr double kAzimuthStepPerTrailStepDeg = 0.5;
// The refraction model stops at this altitude; its correction there is the
// largest value the model reports, so the boundary leaves a step of that size
// in any path that applies refraction while it crosses the edge.
constexpr double kRefractionModelFloorDeg = -1.0;
constexpr double kRefractionEdgeDeg = 0.6466;
// One trail step spans well under a degree of arc, so a smooth projected path
// turns by much less than this at every vertex. A path that absorbs the
// refraction edge turns by about thirty degrees at the vertices around it.
constexpr double kMaxSmoothTurnDeg = 5.0;
constexpr double kEndpointTolerancePx = 1.0e-6;

// Samples a straight altitude and azimuth ramp and applies the correction the
// high-precision refraction model adds above its lower altitude edge. The step
// the model leaves at that edge is what makes a refracted trail turn.
class RefractionEdgeTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit RefractionEdgeTrailEngine(const skygate::core::UtcTimePoint baseUtcTime) : m_baseUtcTime(baseUtcTime) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Refraction edge trail test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineTraits::highPrecisionEngine();
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        (void)request;
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        (void)bodyId;
        return computeBodyState(request, std::size_t{0});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        return stateAt(request.context.utcTime, bodyIndex, request.options.requestsAtmosphericRefraction());
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
        return stateAt(context.utcTime, bodyIndex, false);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    stateAt(const skygate::core::UtcTimePoint utcTime, const std::size_t bodyIndex, const bool applyRefraction) const
    {
        const double offsetMinutes = std::chrono::duration<double, std::ratio<60>>(utcTime - m_baseUtcTime).count();
        const double trailSteps = offsetMinutes / kTrailStepMinutes;
        const double geometricAltitudeDeg = kPresentAltitudeDeg - (kAltitudeStepPerTrailStepDeg * trailSteps);
        const double refractedAltitudeDeg = applyRefraction && geometricAltitudeDeg >= kRefractionModelFloorDeg
                                                ? geometricAltitudeDeg + kRefractionEdgeDeg
                                                : geometricAltitudeDeg;

        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {
                .altitudeDeg = refractedAltitudeDeg,
                .azimuthDeg = kPresentAzimuthDeg + (kAzimuthStepPerTrailStepDeg * trailSteps)
            }
        };
    }

private:
    skygate::core::UtcTimePoint m_baseUtcTime;
};

[[nodiscard]] bool isFutureTrailStroke(const SkyRenderLine& line)
{
    return line.widthPx == 2.0 && line.color.alpha() == 175;
}

[[nodiscard]] bool segmentsConnect(const SkyRenderLine& previous, const SkyRenderLine& next)
{
    return std::abs(previous.x2 - next.x1) <= kEndpointTolerancePx
           && std::abs(previous.y2 - next.y1) <= kEndpointTolerancePx;
}

// Largest direction change between two strokes that share a vertex. Tick
// crosses and the past dashes never share an endpoint with a future stroke.
[[nodiscard]] double largestInteriorTurnDegrees(const std::vector<SkyRenderLine>& lines)
{
    double largestTurnDeg = 0.0;
    std::optional<SkyRenderLine> previousStroke;
    for (const SkyRenderLine& line : lines) {
        if (!isFutureTrailStroke(line) || skygate::ui::tests::TrailRenderQuery::lineLength(line) < 3.0) {
            continue;
        }

        if (previousStroke.has_value() && segmentsConnect(*previousStroke, line)) {
            const double incomingDirectionDeg =
                std::atan2(previousStroke->y2 - previousStroke->y1, previousStroke->x2 - previousStroke->x1);
            const double outgoingDirectionDeg = std::atan2(line.y2 - line.y1, line.x2 - line.x1);
            const double turnDeg =
                std::abs(std::remainder(outgoingDirectionDeg - incomingDirectionDeg, 2.0 * std::numbers::pi_v<double>))
                * 180.0 / std::numbers::pi_v<double>;
            largestTurnDeg = std::max(largestTurnDeg, turnDeg);
        }

        previousStroke = line;
    }

    return largestTurnDeg;
}

}  // namespace

class SkyObjectTrailRefractionKinkTests final : public QObject {
    Q_OBJECT

private slots:
    void starTrailKeepsSmoothPathAcrossRefractionModelEdge();
};

void SkyObjectTrailRefractionKinkTests::starTrailKeepsSmoothPathAcrossRefractionModelEdge()
{
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(
            kViewportWidthPx, kViewportHeightPx, 0.0, kPresentAzimuthDeg, kFieldOfViewDeg
        )
    );
    QVERIFY(projection.has_value());

    const skygate::core::UtcTimePoint baseUtcTime{std::chrono::seconds(1'780'000'000)};
    skygate::ephemeris::EphemerisRequest request;
    request.context.utcTime = baseUtcTime;
    request.context.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
    request.epoch = skygate::core::EpochCodec::epochFromUtcTime(baseUtcTime);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    request.options.setEnableAtmosphericRefraction(true);

    const RefractionEdgeTrailEngine engine(baseUtcTime);
    const std::optional<skygate::ephemeris::CelestialBodyState> presentState = engine.stateAt(baseUtcTime, 0U, true);
    QVERIFY(presentState.has_value());

    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = "trail-refraction-star";
    star.displayName = "Trail refraction star";
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 0.0};
    const skygate::ephemeris::CelestialBodyCatalog catalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{star}
    );

    SkyObjectTrailInput input = skygate::ui::tests::TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::Trail, engine
    );
    input.targetBody = &catalog.bodyAt(0U);
    input.targetState = &*presentState;
    input.targetBodyIndex = 0U;
    input.viewportWidth = kViewportWidthPx;
    input.viewportHeight = kViewportHeightPx;

    const SkyObjectTrailBuilder builder;
    SkyRenderFrame frame;
    builder.appendTrail(frame, input);

    QVERIFY(!frame.lines.empty());
    const double largestTurnDeg = largestInteriorTurnDegrees(frame.lines);
    QVERIFY2(
        largestTurnDeg < kMaxSmoothTurnDeg,
        qPrintable(QStringLiteral("largest interior trail turn is %1 deg").arg(largestTurnDeg))
    );
}

QTEST_APPLESS_MAIN(SkyObjectTrailRefractionKinkTests)

#include "SkyObjectTrailRefractionKinkTests.moc"
