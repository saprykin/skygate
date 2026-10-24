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
// The view centers on the model band so the whole crossing is drawn.
constexpr double kViewCenterAltitudeDeg = -1.5;
constexpr double kViewCenterAzimuthDeg = 205.0;
constexpr double kTrailStepMinutes = 10.0;
constexpr double kPresentAltitudeDeg = 0.55;
constexpr double kPresentAzimuthDeg = 180.0;
// The body moves mostly in azimuth, so its own motion turns the trail by only
// a few degrees per vertex. A correction that steps across the model edge
// becomes a corner of tens of degrees; a tapered one adds a few more degrees.
constexpr double kAltitudeStepPerTrailStepDeg = 0.04;
constexpr double kAzimuthStepPerTrailStepDeg = 0.5;
// The model applies its full correction at the band top and fades it linearly
// to zero at the band bottom. Under the test atmosphere (1010 hPa, 10 degrees
// Celsius) the Bennett fit is 38.8 arcmin at the band top.
constexpr double kRefractionBandTopDeg = -1.0;
constexpr double kRefractionBandBottomDeg = -2.0;
constexpr double kFullCorrectionDeg = 0.6466;
constexpr double kMaxSmoothTurnDeg = 5.0;
constexpr double kEndpointTolerancePx = 1.0e-6;

[[nodiscard]] double taperedCorrectionDeg(const double geometricAltitudeDeg)
{
    if (geometricAltitudeDeg <= kRefractionBandBottomDeg) {
        return 0.0;
    }
    if (geometricAltitudeDeg >= kRefractionBandTopDeg) {
        return kFullCorrectionDeg;
    }

    return kFullCorrectionDeg * (geometricAltitudeDeg - kRefractionBandBottomDeg)
           / (kRefractionBandTopDeg - kRefractionBandBottomDeg);
}

[[nodiscard]] double offsetMinutesAtAltitude(const double geometricAltitudeDeg)
{
    return (kPresentAltitudeDeg - geometricAltitudeDeg) / kAltitudeStepPerTrailStepDeg * kTrailStepMinutes;
}

[[nodiscard]] skygate::core::HorizontalCoordinate trailCoordinateAt(const double offsetMinutes)
{
    const double trailSteps = offsetMinutes / kTrailStepMinutes;
    const double geometricAltitudeDeg = kPresentAltitudeDeg - (kAltitudeStepPerTrailStepDeg * trailSteps);
    return skygate::core::HorizontalCoordinate{
        .altitudeDeg = geometricAltitudeDeg + taperedCorrectionDeg(geometricAltitudeDeg),
        .azimuthDeg = kPresentAzimuthDeg + (kAzimuthStepPerTrailStepDeg * trailSteps)
    };
}

// Samples an altitude and azimuth ramp and applies the correction the
// high-precision refraction model adds across its lower altitude band, so the
// sampled path shows exactly what the taper does to a trail.
class RefractionTaperTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit RefractionTaperTrailEngine(const skygate::core::UtcTimePoint baseUtcTime) : m_baseUtcTime(baseUtcTime) {}

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

    [[nodiscard]] bool sawRefractionRequest() const noexcept
    {
        return m_sawRefractionRequest;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    stateAt(const skygate::core::UtcTimePoint utcTime, const std::size_t bodyIndex, const bool applyRefraction) const
    {
        const double offsetMinutes = std::chrono::duration<double, std::ratio<60>>(utcTime - m_baseUtcTime).count();
        const double trailSteps = offsetMinutes / kTrailStepMinutes;
        const double geometricAltitudeDeg = kPresentAltitudeDeg - (kAltitudeStepPerTrailStepDeg * trailSteps);
        const double refractedAltitudeDeg = applyRefraction && geometricAltitudeDeg > kRefractionBandBottomDeg
                                                ? geometricAltitudeDeg + taperedCorrectionDeg(geometricAltitudeDeg)
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
    void noteRequestedCorrections(const skygate::ephemeris::EphemerisRequest& request) const
    {
        if (request.options.requestsAtmosphericRefraction()) {
            m_sawRefractionRequest = true;
        }
    }

    skygate::core::UtcTimePoint m_baseUtcTime;
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

}  // namespace

class SkyObjectTrailRefractionKinkTests final : public QObject {
    Q_OBJECT

private slots:
    void starTrailStaysSmoothAcrossRefractionTaperBand();
};

void SkyObjectTrailRefractionKinkTests::starTrailStaysSmoothAcrossRefractionTaperBand()
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

    const RefractionTaperTrailEngine engine(baseUtcTime);
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
    QVERIFY(engine.sawRefractionRequest());

    const skygate::core::ScreenPoint bandTopPoint =
        projection->project(trailCoordinateAt(offsetMinutesAtAltitude(kRefractionBandTopDeg)));
    const skygate::core::ScreenPoint bandBottomPoint =
        projection->project(trailCoordinateAt(offsetMinutesAtAltitude(kRefractionBandBottomDeg)));
    QVERIFY(bandTopPoint.isVisible());
    QVERIFY(bandBottomPoint.isVisible());

    const std::optional<DrawnTrailExtent> drawnExtent = futureTrailExtent(frame.lines);
    QVERIFY(drawnExtent.has_value());
    QVERIFY(drawnExtent->topY <= bandTopPoint.y);
    QVERIFY(drawnExtent->bottomY >= bandBottomPoint.y);

    const double largestTurnDeg = largestInteriorTurnDegrees(frame.lines);
    QVERIFY2(
        largestTurnDeg < kMaxSmoothTurnDeg,
        qPrintable(QStringLiteral("largest interior trail turn is %1 deg").arg(largestTurnDeg))
    );
}

QTEST_APPLESS_MAIN(SkyObjectTrailRefractionKinkTests)

#include "SkyObjectTrailRefractionKinkTests.moc"
