#include "EphemerisEngineTestDoubles.hpp"
#include "ObservationEventCalculator.hpp"
#include "ObservationEventTestSupport.hpp"
#include "UtcTimeCodec.hpp"
#include "math/AngleMath.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/CalendarTime.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::tests::kPi;
using skygate::ephemeris::tests::makeContext;
using skygate::ephemeris::tests::makeEngineForBody;
using skygate::ephemeris::tests::makeFixedBody;
using skygate::ephemeris::tests::makeRequest;
using SearchMode = skygate::ephemeris::ObservationEventCalculator::SearchMode;

double currentLocalSiderealHours(const skygate::core::ObservationContext& context)
{
    return skygate::core::AngleMath::normalizeHours(
        skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AstronomicalTime::greenwichMeanSiderealTimeDeg(context.utcTime)
            + context.observer.longitudeDeg
        )
        / 15.0
    );
}

void verifyCrossingAltitude(
    const skygate::ephemeris::IEphemerisEngine& engine,
    skygate::core::ObservationContext context,
    const skygate::core::UtcTimePoint& utcTime,
    const double expectedAltitudeDeg = 0.0
)
{
    context.utcTime = utcTime;
    const auto state = engine.computeBodyState(context, 0U);
    QVERIFY(state.has_value());
    QVERIFY(std::abs(state->horizontal.altitudeDeg - expectedAltitudeDeg) < 0.02);
}

class MovingBodyEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Moving body test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
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
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
            std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{makeFixedBody({})}
        );
        snapshot.states.push_back(*computeBodyState(context, 0U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++sampleCount;
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(context.utcTime);
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = 35.0 * std::sin(2.0 * kPi * (phase - 0.25)), .azimuthDeg = 180.0}
        };
    }

    mutable int sampleCount = 0;
};

class RequestSensitiveMovingEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return engineOptions.engineKind();
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Request-sensitive moving test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
            std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{makeFixedBody({})}
        );
        snapshot.states.push_back(*computeBodyState(request, std::size_t{0U}));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++requestSampleCount;
        sawLightTimeRequest =
            sawLightTimeRequest
            || skygate::ephemeris::EphemerisCorrectionFlags::has(
                request.options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
            );
        sawNoCorrectionsRequest =
            sawNoCorrectionsRequest
            || request.options.correctionFlags() == skygate::ephemeris::EphemerisCorrectionFlags::noCorrections();
        if (request.context.utcTime != baseUtcTime
            && (request.epoch.julianDatePart1 != baseEpoch.julianDatePart1
                || request.epoch.julianDatePart2 != baseEpoch.julianDatePart2)) {
            sawSampleEpochUpdate = true;
        }

        const double altitudeDeg =
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                request.options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
            )
                ? movingAltitudeDeg(request.context.utcTime)
                : -20.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = 180.0}
        };
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::string_view) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return engineOptions;
    }

    static double movingAltitudeDeg(const skygate::core::UtcTimePoint& utcTime) noexcept
    {
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime);
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return 35.0 * std::sin(2.0 * kPi * (phase - 0.25));
    }

    skygate::ephemeris::EphemerisEngineOptions engineOptions;
    skygate::core::UtcTimePoint baseUtcTime{};
    skygate::core::AstronomicalEpoch baseEpoch;
    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
    mutable bool sawLightTimeRequest = false;
    mutable bool sawNoCorrectionsRequest = false;
    mutable bool sawSampleEpochUpdate = false;
};

}  // namespace

class ObservationEventDirectSearchTests final : public QObject {
    Q_OBJECT

private slots:
    void normalObjectFindsOrderedRefinedCrossings();
    void circumpolarNeverRisingFallbacksWithCulmination();
    void currentAboveHorizonSetsBeforeItRisesAgain();
    void currentBelowHorizonRisesBeforeItSetsAgain();
    void configurableThresholdFindsDifferentCrossings();
    void invalidAndUnresolvedInputsReturnExplicitStatuses();
    void unprovenWindowMissDoesNotReportAlwaysAboveBelow();
    void movingBodySamplesThroughEphemerisEngine();
    void requestOverloadPropagatesOptionsAndSampleEpochs();
    void contextOverloadSeedsRequestOptionsFromEngine();
};

void ObservationEventDirectSearchTests::normalObjectFindsOrderedRefinedCrossings()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const auto context = makeContext();
    const auto engine = makeEngineForBody(makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0}));
    QVERIFY(engine != nullptr);

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(summary.culmination.utcTime.has_value());
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.nextRise.utcTime > context.utcTime);
    QVERIFY(*summary.nextSet.utcTime > context.utcTime);
    QVERIFY(*summary.culmination.utcTime > context.utcTime);
    QVERIFY(*summary.culmination.altitudeDeg > 60.0);

    verifyCrossingAltitude(*engine, context, *summary.nextRise.utcTime);
    verifyCrossingAltitude(*engine, context, *summary.nextSet.utcTime);
}

void ObservationEventDirectSearchTests::circumpolarNeverRisingFallbacksWithCulmination()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);

    const auto circumpolarContext = makeContext(60.0, 0.0);
    const auto circumpolarBody = makeFixedBody({.rightAscensionHours = 3.0, .declinationDeg = 80.0});
    auto engine = makeEngineForBody(circumpolarBody);
    auto request = makeRequest(circumpolarContext, *engine);
    auto summary = calculator.compute(*engine, request, 0U, &circumpolarBody, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 65.0);

    const auto neverRisingContext = makeContext(60.0, 0.0);
    const auto neverRisingBody = makeFixedBody({.rightAscensionHours = 3.0, .declinationDeg = -80.0});
    engine = makeEngineForBody(neverRisingBody);
    request = makeRequest(neverRisingContext, *engine);
    summary = calculator.compute(*engine, request, 0U, &neverRisingBody, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysBelow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysBelow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg < -25.0);
}

void ObservationEventDirectSearchTests::currentAboveHorizonSetsBeforeItRisesAgain()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(
        makeFixedBody({.rightAscensionHours = currentLocalSiderealHours(context), .declinationDeg = 0.0})
    );

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*summary.nextSet.utcTime < *summary.nextRise.utcTime);
}

void ObservationEventDirectSearchTests::currentBelowHorizonRisesBeforeItSetsAgain()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(makeFixedBody(
        {.rightAscensionHours = skygate::core::AngleMath::normalizeHours(currentLocalSiderealHours(context) + 12.0),
         .declinationDeg = 0.0}
    ));

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*summary.nextRise.utcTime < *summary.nextSet.utcTime);
}

void ObservationEventDirectSearchTests::configurableThresholdFindsDifferentCrossings()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(makeFixedBody(
        {.rightAscensionHours = skygate::core::AngleMath::normalizeHours(currentLocalSiderealHours(context) + 12.0),
         .declinationDeg = 0.0}
    ));

    const auto request = makeRequest(context, *engine);
    const auto horizonSummary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    const auto twilightSummary = calculator.compute(*engine, request, 0U, nullptr, -6.0, SearchMode::Guided);

    QCOMPARE(horizonSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(twilightSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(horizonSummary.nextRise.utcTime.has_value());
    QVERIFY(twilightSummary.nextRise.utcTime.has_value());
    QVERIFY(*twilightSummary.nextRise.utcTime < *horizonSummary.nextRise.utcTime);

    verifyCrossingAltitude(*engine, context, *horizonSummary.nextRise.utcTime, 0.0);
    verifyCrossingAltitude(*engine, context, *twilightSummary.nextRise.utcTime, -6.0);

    QVERIFY(horizonSummary.nextRise.altitudeDeg.has_value());
    QCOMPARE(*horizonSummary.nextRise.altitudeDeg, 0.0);
    QVERIFY(horizonSummary.nextSet.altitudeDeg.has_value());
    QCOMPARE(*horizonSummary.nextSet.altitudeDeg, 0.0);
    QVERIFY(twilightSummary.nextRise.altitudeDeg.has_value());
    QCOMPARE(*twilightSummary.nextRise.altitudeDeg, -6.0);
    QVERIFY(twilightSummary.nextSet.altitudeDeg.has_value());
    QCOMPARE(*twilightSummary.nextSet.altitudeDeg, -6.0);
}

void ObservationEventDirectSearchTests::invalidAndUnresolvedInputsReturnExplicitStatuses()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    auto context = makeContext();
    auto engine = makeEngineForBody(makeFixedBody({.rightAscensionHours = 1.0, .declinationDeg = 10.0}));

    context.observer.latitudeDeg = 120.0;
    auto request = makeRequest(context, *engine);
    auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);

    context = makeContext();
    skygate::ephemeris::OwnGalaxyCelestialBody unresolved;
    unresolved.id = "unresolved";
    unresolved.displayName = "Unresolved";
    unresolved.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    engine = makeEngineForBody(unresolved);
    request = makeRequest(context, *engine);
    summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Unresolved);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Unresolved);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Unresolved);
}

void ObservationEventDirectSearchTests::unprovenWindowMissDoesNotReportAlwaysAboveBelow()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const skygate::ephemeris::tests::FixedAltitudeEngine belowHorizonEngine(-20.0);
    const skygate::ephemeris::tests::FixedAltitudeEngine aboveHorizonEngine(20.0);
    const auto context = makeContext(70.0, 0.0);

    auto request = makeRequest(context, belowHorizonEngine);
    auto summary = calculator.compute(belowHorizonEngine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);

    request = makeRequest(context, aboveHorizonEngine);
    summary = calculator.compute(aboveHorizonEngine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
}

void ObservationEventDirectSearchTests::movingBodySamplesThroughEphemerisEngine()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const MovingBodyEngine engine;
    auto context = makeContext(0.0, 0.0);
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));

    const auto request = makeRequest(context, engine);
    const auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QVERIFY(engine.sampleCount > 300);
    QVERIFY(engine.sampleCount < 500);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 34.9);
}

void ObservationEventDirectSearchTests::requestOverloadPropagatesOptionsAndSampleEpochs()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    RequestSensitiveMovingEngine engine;
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext(0.0, 0.0);
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 1970,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());
    engine.baseUtcTime = request.context.utcTime;
    engine.baseEpoch = request.epoch;

    auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);

    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
    summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount, 0);
    QVERIFY(engine.requestSampleCount > 300);
    QVERIFY(engine.sawLightTimeRequest);
    QVERIFY(engine.sawSampleEpochUpdate);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
}

void ObservationEventDirectSearchTests::contextOverloadSeedsRequestOptionsFromEngine()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    RequestSensitiveMovingEngine engine;
    engine.engineOptions.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    engine.engineOptions.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());
    auto context = makeContext(0.0, 0.0);
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));

    const auto request = makeRequest(context, engine);
    const auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount, 0);
    QVERIFY(engine.requestSampleCount > 0);
    QVERIFY(engine.sawNoCorrectionsRequest);
    QVERIFY(!engine.sawLightTimeRequest);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::NoEventInSearchWindow);
}

QTEST_APPLESS_MAIN(ObservationEventDirectSearchTests)

#include "ObservationEventDirectSearchTests.moc"
