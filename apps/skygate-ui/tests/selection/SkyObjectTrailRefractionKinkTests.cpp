#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
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
// The view centers on the model band so the whole crossing is drawn.
constexpr double kViewCenterAltitudeDeg = -1.5;
constexpr double kViewCenterAzimuthDeg = 205.0;
constexpr double kTrailStepMinutes = 10.0;
// The band the refraction model tapers its correction across, from the full
// Bennett fit at the top to nothing at the bottom.
constexpr double kRefractionBandTopDeg = -1.0;
constexpr double kRefractionBandBottomDeg = -2.0;
constexpr double kEndpointTolerancePx = 1.0e-6;

// Where the drawn body sits at the selected time and how far it travels in
// altitude and azimuth between two render samples.
struct TrailGeometry final {
    double startAltitudeDeg = 0.0;
    double startAzimuthDeg = 180.0;
    double altitudeStepDeg = 0.0;
    double azimuthStepDeg = 0.5;
};

// The reported steep trail: 0.3 degrees of altitude and 0.5 degrees of azimuth
// every ten minutes, about 31 degrees away from the azimuth axis. Only three
// or four samples fall inside the one-degree band, so the taper's own bend
// already turns this trail by about 14 degrees at its worst vertex. A
// correction cut off at the band top turns it by 35.
constexpr TrailGeometry kSteepTrail{
    .startAltitudeDeg = 0.55,
    .startAzimuthDeg = 180.0,
    .altitudeStepDeg = 0.3,
    .azimuthStepDeg = 0.5,
};
// A shallow trail crosses the same band over four hours, which gives the
// taper twenty-five vertices to bend across and drops the worst turn to under
// a degree. The steep crossing samples the band too coarsely to show that
// difference, so both crossings are drawn.
constexpr TrailGeometry kShallowTrail{
    .startAltitudeDeg = 0.55,
    .startAzimuthDeg = 180.0,
    .altitudeStepDeg = 0.04,
    .azimuthStepDeg = 0.5,
};

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeTrailStar()
{
    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = "trail-refraction-star";
    star.displayName = "Trail refraction star";
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 6.0, .declinationDeg = 0.0};
    return star;
}

// Runs the production refraction model on a single geometric altitude, so the
// sampled trail carries the correction shape the renderer actually sees. The
// model leaves azimuth alone, so the azimuth handed to it is a placeholder.
[[nodiscard]] double
refractedAltitudeDeg(const skygate::ephemeris::EphemerisRequest& request, const double geometricAltitudeDeg)
{
    static const skygate::ephemeris::OwnGalaxyCelestialBody kBody = makeTrailStar();
    skygate::ephemeris::highprecision::HighPrecisionCalculatorResult geometricResult;
    geometricResult.horizontal = skygate::core::HorizontalCoordinate{
        .altitudeDeg = geometricAltitudeDeg,
        .azimuthDeg = 0.0,
    };
    const skygate::ephemeris::highprecision::AtmosphericRefractionCalculator calculator;
    const skygate::ephemeris::highprecision::HighPrecisionCalculatorResult refractedResult = calculator.apply(
        skygate::ephemeris::highprecision::HighPrecisionComputationInput{
            .request = request,
            .body = kBody,
            .bodyIndex = 0U,
        },
        geometricResult
    );
    return refractedResult.horizontal->altitudeDeg;
}

[[nodiscard]] double offsetMinutesAtAltitude(const TrailGeometry& geometry, const double geometricAltitudeDeg)
{
    return (geometry.startAltitudeDeg - geometricAltitudeDeg) / geometry.altitudeStepDeg * kTrailStepMinutes;
}

[[nodiscard]] skygate::core::HorizontalCoordinate trailCoordinateAt(
    const skygate::ephemeris::EphemerisRequest& request, const TrailGeometry& geometry, const double offsetMinutes
)
{
    const double trailSteps = offsetMinutes / kTrailStepMinutes;
    const double geometricAltitudeDeg = geometry.startAltitudeDeg - (geometry.altitudeStepDeg * trailSteps);
    return skygate::core::HorizontalCoordinate{
        .altitudeDeg = refractedAltitudeDeg(request, geometricAltitudeDeg),
        .azimuthDeg = geometry.startAzimuthDeg + (geometry.azimuthStepDeg * trailSteps)
    };
}

// Samples an altitude and azimuth ramp and applies the production refraction
// model to it, so the drawn path shows exactly what the taper does to a trail.
class RefractionTaperTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    RefractionTaperTrailEngine(const skygate::core::UtcTimePoint baseUtcTime, const TrailGeometry& geometry)
        : m_baseUtcTime(baseUtcTime), m_geometry(geometry)
    {
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Refraction taper trail test engine";
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
        noteRequestedCorrections(request);
        return stateAt(request.context.utcTime, bodyIndex, &request);
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
        return stateAt(context.utcTime, bodyIndex, nullptr);
    }

    [[nodiscard]] bool sawRefractionRequest() const noexcept
    {
        return m_sawRefractionRequest;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> stateAt(
        const skygate::core::UtcTimePoint utcTime,
        const std::size_t bodyIndex,
        const skygate::ephemeris::EphemerisRequest* request
    ) const
    {
        const double offsetMinutes = std::chrono::duration<double, std::ratio<60>>(utcTime - m_baseUtcTime).count();
        const double trailSteps = offsetMinutes / kTrailStepMinutes;
        const double geometricAltitudeDeg = m_geometry.startAltitudeDeg - (m_geometry.altitudeStepDeg * trailSteps);

        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {
                .altitudeDeg =
                    request != nullptr ? refractedAltitudeDeg(*request, geometricAltitudeDeg) : geometricAltitudeDeg,
                .azimuthDeg = m_geometry.startAzimuthDeg + (m_geometry.azimuthStepDeg * trailSteps)
            }
        };
    }

private:
    void noteRequestedCorrections(const skygate::ephemeris::EphemerisRequest& request) const
    {
        if (request.options.requestsAtmosphericRefraction()) {
            m_sawRefractionRequest = true;
        }
    }

    skygate::core::UtcTimePoint m_baseUtcTime;
    TrailGeometry m_geometry;
    mutable bool m_sawRefractionRequest = false;
};

// Vertical extent of the drawn future trail in screen pixels. Screen y grows
// downward, so topY belongs to the highest altitude the trail reaches.
struct DrawnTrailExtent final {
    double topY = 0.0;
    double bottomY = 0.0;
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

[[nodiscard]] std::optional<DrawnTrailExtent> futureTrailExtent(const std::vector<SkyRenderLine>& lines)
{
    std::optional<DrawnTrailExtent> extent;
    for (const SkyRenderLine& line : lines) {
        if (!isFutureTrailStroke(line)) {
            continue;
        }

        const double strokeTopY = std::min(line.y1, line.y2);
        const double strokeBottomY = std::max(line.y1, line.y2);
        if (!extent.has_value()) {
            extent = DrawnTrailExtent{.topY = strokeTopY, .bottomY = strokeBottomY};
            continue;
        }

        extent->topY = std::min(extent->topY, strokeTopY);
        extent->bottomY = std::max(extent->bottomY, strokeBottomY);
    }
    return extent;
}

// Draws one trail through the model band and checks that it stays connected
// across the whole crossing and never turns by a visible corner. A failing
// check records the failure and leaves this helper.
void trailStaysSmooth(const TrailGeometry& geometry, const double maxTurnDeg)
{
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(
            kViewportWidthPx, kViewportHeightPx, kViewCenterAltitudeDeg, kViewCenterAzimuthDeg, kFieldOfViewDeg
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

    const RefractionTaperTrailEngine engine(baseUtcTime, geometry);

    SkyObjectTrailInput input = skygate::ui::tests::TrailBuilderTestScenario::makeInput(engine, *projection);
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::Trail, engine
    );
    const std::optional<skygate::ephemeris::CelestialBodyState> presentState =
        engine.stateAt(baseUtcTime, 0U, &*input.ephemerisRequest);
    QVERIFY(presentState.has_value());

    const skygate::ephemeris::CelestialBodyCatalog catalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{makeTrailStar()}
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
    QVERIFY(engine.sawRefractionRequest());

    const skygate::core::ScreenPoint bandTopPoint = projection->project(
        trailCoordinateAt(*input.ephemerisRequest, geometry, offsetMinutesAtAltitude(geometry, kRefractionBandTopDeg))
    );
    const skygate::core::ScreenPoint bandBottomPoint = projection->project(trailCoordinateAt(
        *input.ephemerisRequest, geometry, offsetMinutesAtAltitude(geometry, kRefractionBandBottomDeg)
    ));
    QVERIFY(bandTopPoint.isVisible());
    QVERIFY(bandBottomPoint.isVisible());

    const std::optional<DrawnTrailExtent> drawnExtent = futureTrailExtent(frame.lines);
    QVERIFY(drawnExtent.has_value());
    QVERIFY(drawnExtent->topY <= bandTopPoint.y);
    QVERIFY(drawnExtent->bottomY >= bandBottomPoint.y);

    const double largestTurnDeg = largestInteriorTurnDegrees(frame.lines);
    QVERIFY2(
        largestTurnDeg < maxTurnDeg,
        qPrintable(
            QStringLiteral("largest interior trail turn is %1 deg, allowed %2 deg").arg(largestTurnDeg).arg(maxTurnDeg)
        )
    );
}

}  // namespace

class SkyObjectTrailRefractionKinkTests final : public QObject {
    Q_OBJECT

private slots:
    void steepTrailStaysSmoothAcrossTaperBand();
    void shallowTrailStaysSmoothAcrossTaperBand();
};

void SkyObjectTrailRefractionKinkTests::steepTrailStaysSmoothAcrossTaperBand()
{
    // Three or four samples across the band leave the fade turning this trail
    // by about 14 degrees; a correction cut off at the band top turns it by 35.
    trailStaysSmooth(kSteepTrail, 16.0);
}

void SkyObjectTrailRefractionKinkTests::shallowTrailStaysSmoothAcrossTaperBand()
{
    // The fade turns this trail by less than a degree. A ramp across the band
    // turns it by two and a half and a hard edge by fifty.
    trailStaysSmooth(kShallowTrail, 1.5);
}

QTEST_APPLESS_MAIN(SkyObjectTrailRefractionKinkTests)

#include "SkyObjectTrailRefractionKinkTests.moc"
