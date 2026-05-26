#include "ObservationEventCalculator.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "EphemerisRequestFactory.hpp"
#include "UtcTimeCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "math/AngleMath.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/CalendarTime.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

skygate::core::SkyContext makeContext(const double latitudeDeg = 47.0, const double longitudeDeg = 8.0)
{
    skygate::core::SkyContext context;
    context.observer = {.latitudeDeg = latitudeDeg, .longitudeDeg = longitudeDeg, .elevationMeters = 400.0};
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return context;
}

skygate::ephemeris::CelestialBody makeFixedBody(const skygate::core::EquatorialCoordinate& equatorial)
{
    skygate::ephemeris::CelestialBody body;
    body.id = "target";
    body.displayName = "Target";
    body.type = skygate::ephemeris::CelestialBodyType::Star;
    body.visualMagnitude = 1.0;
    body.fixedEquatorial = equatorial;
    return body;
}

skygate::ephemeris::EphemerisRequest
makeRequest(const skygate::core::SkyContext& context, const skygate::ephemeris::IEphemerisEngine& engine)
{
    return skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engine.options());
}

std::unique_ptr<skygate::ephemeris::IEphemerisEngine> makeEngineForBody(const skygate::ephemeris::CelestialBody& body)
{
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({body});
    Q_ASSERT(catalog != nullptr);
    return std::move(skygate::ephemeris::EphemerisEngineFactory::create(*catalog).engine);
}

double currentLocalSiderealHours(const skygate::core::SkyContext& context)
{
    return skygate::core::AngleMath::normalizeHours(
        skygate::core::AngleMath::normalizeDegrees(
            skygate::ephemeris::AstronomicalTime::greenwichMeanSiderealTimeDeg(context.utcTime)
            + context.observer.longitudeDeg
        )
        / 15.0
    );
}

void verifyCrossingAltitude(
    const skygate::ephemeris::IEphemerisEngine& engine,
    skygate::core::SkyContext context,
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
    [[nodiscard]] skygate::ephemeris::SkySnapshot
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
        return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
            std::vector<skygate::ephemeris::CelestialBody>{makeFixedBody({})}
        );
        snapshot.states.push_back(*computeBodyState(context, 0U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
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
    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
            std::vector<skygate::ephemeris::CelestialBody>{makeFixedBody({})}
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
            || hasCorrectionFlag(
                request.options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
            );
        sawNoCorrectionsRequest =
            sawNoCorrectionsRequest
            || request.options.correctionFlags == skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
        if (request.context.utcTime != baseUtcTime
            && (request.epoch.julianDatePart1 != baseEpoch.julianDatePart1
                || request.epoch.julianDatePart2 != baseEpoch.julianDatePart2)) {
            sawSampleEpochUpdate = true;
        }

        const double altitudeDeg =
            hasCorrectionFlag(request.options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime)
                ? movingAltitudeDeg(request.context.utcTime)
                : -20.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = 180.0}
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
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
    skygate::ephemeris::AstronomicalEpoch baseEpoch;
    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
    mutable bool sawLightTimeRequest = false;
    mutable bool sawNoCorrectionsRequest = false;
    mutable bool sawSampleEpochUpdate = false;
};

using SearchMode = skygate::ephemeris::ObservationEventCalculator::SearchMode;

}  // namespace

class ObservationEventCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void normalObjectFindsOrderedEventsAndRefinedHorizonCrossings();
    void circumpolarAndNeverRisingObjectsReportFallbacksButStillCulminate();
    void currentAboveHorizonSetsBeforeItRisesAgain();
    void currentBelowHorizonRisesBeforeItSetsAgain();
    void configurableAltitudeThresholdFindsDifferentCrossings();
    void invalidAndUnresolvedInputsReturnExplicitStatuses();
    void unprovenWindowMissDoesNotReportAlwaysAboveOrBelow();
    void movingBodySamplesThroughEphemerisEngine();
    void highPrecisionFixedBodyUsesGuidedCoarseSearch();
    void requestOverloadPropagatesOptionsAndSampleEpochs();
    void contextOverloadSeedsRequestOptionsFromEngine();
};

void ObservationEventCalculatorTests::normalObjectFindsOrderedEventsAndRefinedHorizonCrossings()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto context = makeContext();
    const auto engine = makeEngineForBody(makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0}));
    QVERIFY(engine != nullptr);

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
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

void ObservationEventCalculatorTests::circumpolarAndNeverRisingObjectsReportFallbacksButStillCulminate()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;

    const auto circumpolarContext = makeContext(60.0, 0.0);
    const auto circumpolarBody = makeFixedBody({.rightAscensionHours = 3.0, .declinationDeg = 80.0});
    auto engine = makeEngineForBody(circumpolarBody);
    auto request = makeRequest(circumpolarContext, *engine);
    auto summary = calculator.compute(*engine, request, 0U, &circumpolarBody, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::AlwaysAbove);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::AlwaysAbove);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 65.0);

    const auto neverRisingContext = makeContext(60.0, 0.0);
    const auto neverRisingBody = makeFixedBody({.rightAscensionHours = 3.0, .declinationDeg = -80.0});
    engine = makeEngineForBody(neverRisingBody);
    request = makeRequest(neverRisingContext, *engine);
    summary = calculator.compute(*engine, request, 0U, &neverRisingBody, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::AlwaysBelow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::AlwaysBelow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg < -25.0);
}

void ObservationEventCalculatorTests::currentAboveHorizonSetsBeforeItRisesAgain()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(
        makeFixedBody({.rightAscensionHours = currentLocalSiderealHours(context), .declinationDeg = 0.0})
    );

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*summary.nextSet.utcTime < *summary.nextRise.utcTime);
}

void ObservationEventCalculatorTests::currentBelowHorizonRisesBeforeItSetsAgain()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(makeFixedBody(
        {.rightAscensionHours = skygate::core::AngleMath::normalizeHours(currentLocalSiderealHours(context) + 12.0),
         .declinationDeg = 0.0}
    ));

    const auto request = makeRequest(context, *engine);
    const auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*summary.nextRise.utcTime < *summary.nextSet.utcTime);
}

void ObservationEventCalculatorTests::configurableAltitudeThresholdFindsDifferentCrossings()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto context = makeContext(0.0, 0.0);
    const auto engine = makeEngineForBody(makeFixedBody(
        {.rightAscensionHours = skygate::core::AngleMath::normalizeHours(currentLocalSiderealHours(context) + 12.0),
         .declinationDeg = 0.0}
    ));

    const auto request = makeRequest(context, *engine);
    const auto horizonSummary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    const auto twilightSummary = calculator.compute(*engine, request, 0U, nullptr, -6.0, SearchMode::Guided);

    QCOMPARE(horizonSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(twilightSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
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

void ObservationEventCalculatorTests::invalidAndUnresolvedInputsReturnExplicitStatuses()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    auto context = makeContext();
    auto engine = makeEngineForBody(makeFixedBody({.rightAscensionHours = 1.0, .declinationDeg = 10.0}));

    context.observer.latitudeDeg = 120.0;
    auto request = makeRequest(context, *engine);
    auto summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);

    context = makeContext();
    skygate::ephemeris::CelestialBody unresolved;
    unresolved.id = "unresolved";
    unresolved.displayName = "Unresolved";
    unresolved.type = skygate::ephemeris::CelestialBodyType::DeepSkyObject;
    engine = makeEngineForBody(unresolved);
    request = makeRequest(context, *engine);
    summary = calculator.compute(*engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
}

void ObservationEventCalculatorTests::unprovenWindowMissDoesNotReportAlwaysAboveOrBelow()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const skygate::ephemeris::tests::FixedAltitudeEngine belowHorizonEngine(-20.0);
    const skygate::ephemeris::tests::FixedAltitudeEngine aboveHorizonEngine(20.0);
    const auto context = makeContext(70.0, 0.0);

    auto request = makeRequest(context, belowHorizonEngine);
    auto summary = calculator.compute(belowHorizonEngine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);

    request = makeRequest(context, aboveHorizonEngine);
    summary = calculator.compute(aboveHorizonEngine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
}

void ObservationEventCalculatorTests::movingBodySamplesThroughEphemerisEngine()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const MovingBodyEngine engine;
    auto context = makeContext(0.0, 0.0);
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));

    const auto request = makeRequest(context, engine);
    const auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QVERIFY(engine.sampleCount > 300);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 34.9);
}

void ObservationEventCalculatorTests::highPrecisionFixedBodyUsesGuidedCoarseSearch()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    const skygate::ephemeris::tests::RequestCountingEphemerisEngine engine(
        makeEngineForBody(body), skygate::ephemeris::tests::highPrecisionLightTimeOptions()
    );
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.epoch = *skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 2024,
            .month = 6,
            .day = 2,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    request.options = engine.options();

    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount(), 0);
    QCOMPARE(engine.requestSampleCount(), 0);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
}

void ObservationEventCalculatorTests::requestOverloadPropagatesOptionsAndSampleEpochs()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    RequestSensitiveMovingEngine engine;
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext(0.0, 0.0);
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));
    request.epoch = *skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 1970,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
    engine.baseUtcTime = request.context.utcTime;
    engine.baseEpoch = request.epoch;

    auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);

    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount, 0);
    QVERIFY(engine.requestSampleCount > 300);
    QVERIFY(engine.sawLightTimeRequest);
    QVERIFY(engine.sawSampleEpochUpdate);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
}

void ObservationEventCalculatorTests::contextOverloadSeedsRequestOptionsFromEngine()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    RequestSensitiveMovingEngine engine;
    engine.engineOptions.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    engine.engineOptions.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
    auto context = makeContext(0.0, 0.0);
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));

    const auto request = makeRequest(context, engine);
    const auto summary = calculator.compute(engine, request, 0U, nullptr, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount, 0);
    QVERIFY(engine.requestSampleCount > 0);
    QVERIFY(engine.sawNoCorrectionsRequest);
    QVERIFY(!engine.sawLightTimeRequest);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow);
}

QTEST_APPLESS_MAIN(ObservationEventCalculatorTests)

#include "ObservationEventCalculatorTests.moc"
