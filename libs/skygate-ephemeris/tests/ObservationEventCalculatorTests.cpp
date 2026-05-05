#include "engine/JulianDateTime.hpp"
#include "skygate/core/math/AngleMath.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

skygate::core::SkyContext makeContext(
    const double latitudeDeg = 47.0,
    const double longitudeDeg = 8.0
)
{
    skygate::core::SkyContext context;
    context.observer = {
        .latitudeDeg = latitudeDeg,
        .longitudeDeg = longitudeDeg,
        .elevationMeters = 400.0
    };
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return context;
}

skygate::ephemeris::CelestialBody makeFixedBody(
    const skygate::core::EquatorialCoordinate& equatorial
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = "target";
    body.displayName = "Target";
    body.type = skygate::ephemeris::CelestialBodyType::Star;
    body.visualMagnitude = 1.0;
    body.fixedEquatorial = equatorial;
    return body;
}

std::unique_ptr<skygate::ephemeris::IEphemerisEngine> makeEngineForBody(
    const skygate::ephemeris::CelestialBody& body
)
{
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies({body});
    Q_ASSERT(catalog != nullptr);
    return skygate::ephemeris::createEphemerisEngine(*catalog);
}

double currentLocalSiderealHours(const skygate::core::SkyContext& context)
{
    return skygate::core::AngleMath::normalizeHours(
        skygate::core::AngleMath::normalizeDegrees(
            skygate::ephemeris::JulianDateTime::greenwichMeanSiderealTimeDeg(context.utcTime)
            + context.observer.longitudeDeg
        ) / 15.0
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
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies =
            std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
                std::vector<skygate::ephemeris::CelestialBody> {makeFixedBody({})}
            );
        snapshot.states.push_back(*computeBodyState(context, 0U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++sampleCount;
        const double seconds = static_cast<double>(context.utcTime.time_since_epoch().count());
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return skygate::ephemeris::CelestialBodyState {
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {
                .altitudeDeg = 35.0 * std::sin(2.0 * kPi * (phase - 0.25)),
                .azimuthDeg = 180.0
            }
        };
    }

    mutable int sampleCount = 0;
};

class ConstantAltitudeEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit ConstantAltitudeEngine(const double altitudeDeg) :
        m_altitudeDeg(altitudeDeg)
    {
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies =
            std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
                std::vector<skygate::ephemeris::CelestialBody> {makeFixedBody({})}
            );
        snapshot.states.push_back(*computeBodyState(context, 0U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext&,
        const std::uint32_t bodyIndex
    ) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        return skygate::ephemeris::CelestialBodyState {
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = m_altitudeDeg, .azimuthDeg = 180.0}
        };
    }

private:
    double m_altitudeDeg = 0.0;
};

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
};

void ObservationEventCalculatorTests::normalObjectFindsOrderedEventsAndRefinedHorizonCrossings()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto context = makeContext();
    const auto engine = makeEngineForBody(makeFixedBody({
        .rightAscensionHours = 8.0,
        .declinationDeg = 20.0
    }));
    QVERIFY(engine != nullptr);

    const auto summary = calculator.compute(*engine, context, 0U);

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
    const auto circumpolarBody = makeFixedBody({
        .rightAscensionHours = 3.0,
        .declinationDeg = 80.0
    });
    auto engine = makeEngineForBody(circumpolarBody);
    auto summary = calculator.compute(*engine, circumpolarContext, 0U, circumpolarBody);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::AlwaysAbove);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::AlwaysAbove);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 65.0);

    const auto neverRisingContext = makeContext(60.0, 0.0);
    const auto neverRisingBody = makeFixedBody({
        .rightAscensionHours = 3.0,
        .declinationDeg = -80.0
    });
    engine = makeEngineForBody(neverRisingBody);
    summary = calculator.compute(*engine, neverRisingContext, 0U, neverRisingBody);
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
    const auto engine = makeEngineForBody(makeFixedBody({
        .rightAscensionHours = currentLocalSiderealHours(context),
        .declinationDeg = 0.0
    }));

    const auto summary = calculator.compute(*engine, context, 0U);

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
    const auto engine = makeEngineForBody(makeFixedBody({
        .rightAscensionHours = skygate::core::AngleMath::normalizeHours(
            currentLocalSiderealHours(context) + 12.0
        ),
        .declinationDeg = 0.0
    }));

    const auto summary = calculator.compute(*engine, context, 0U);

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
    const auto engine = makeEngineForBody(makeFixedBody({
        .rightAscensionHours = skygate::core::AngleMath::normalizeHours(
            currentLocalSiderealHours(context) + 12.0
        ),
        .declinationDeg = 0.0
    }));

    const auto horizonSummary = calculator.compute(*engine, context, 0U, 0.0);
    const auto twilightSummary = calculator.compute(*engine, context, 0U, -6.0);

    QCOMPARE(horizonSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(twilightSummary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(horizonSummary.nextRise.utcTime.has_value());
    QVERIFY(twilightSummary.nextRise.utcTime.has_value());
    QVERIFY(*twilightSummary.nextRise.utcTime < *horizonSummary.nextRise.utcTime);

    verifyCrossingAltitude(*engine, context, *horizonSummary.nextRise.utcTime, 0.0);
    verifyCrossingAltitude(*engine, context, *twilightSummary.nextRise.utcTime, -6.0);
}

void ObservationEventCalculatorTests::invalidAndUnresolvedInputsReturnExplicitStatuses()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    auto context = makeContext();
    auto engine = makeEngineForBody(makeFixedBody({
        .rightAscensionHours = 1.0,
        .declinationDeg = 10.0
    }));

    context.observer.latitudeDeg = 120.0;
    auto summary = calculator.compute(*engine, context, 0U);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::InvalidInput);

    context = makeContext();
    skygate::ephemeris::CelestialBody unresolved;
    unresolved.id = "unresolved";
    unresolved.displayName = "Unresolved";
    unresolved.type = skygate::ephemeris::CelestialBodyType::DeepSkyObject;
    engine = makeEngineForBody(unresolved);
    summary = calculator.compute(*engine, context, 0U);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
}

void ObservationEventCalculatorTests::unprovenWindowMissDoesNotReportAlwaysAboveOrBelow()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const ConstantAltitudeEngine belowHorizonEngine(-20.0);
    const ConstantAltitudeEngine aboveHorizonEngine(20.0);
    const auto context = makeContext(70.0, 0.0);

    auto summary = calculator.compute(belowHorizonEngine, context, 0U);
    QCOMPARE(
        summary.nextRise.status,
        skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow
    );
    QCOMPARE(
        summary.nextSet.status,
        skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow
    );
    QCOMPARE(
        summary.culmination.status,
        skygate::ephemeris::ObservationEventStatus::Available
    );

    summary = calculator.compute(aboveHorizonEngine, context, 0U);
    QCOMPARE(
        summary.nextRise.status,
        skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow
    );
    QCOMPARE(
        summary.nextSet.status,
        skygate::ephemeris::ObservationEventStatus::NoEventInSearchWindow
    );
    QCOMPARE(
        summary.culmination.status,
        skygate::ephemeris::ObservationEventStatus::Available
    );
}

void ObservationEventCalculatorTests::movingBodySamplesThroughEphemerisEngine()
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const MovingBodyEngine engine;
    auto context = makeContext(0.0, 0.0);
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));

    const auto summary = calculator.compute(engine, context, 0U);

    QVERIFY(engine.sampleCount > 300);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.culmination.altitudeDeg.has_value());
    QVERIFY(*summary.culmination.altitudeDeg > 34.9);
}

QTEST_APPLESS_MAIN(ObservationEventCalculatorTests)

#include "ObservationEventCalculatorTests.moc"
