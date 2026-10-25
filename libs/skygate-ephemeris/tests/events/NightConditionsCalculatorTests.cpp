#include "EphemerisEngineTestDoubles.hpp"
#include "EphemerisRequestFactory.hpp"
#include "MarkerGuidanceStrategy.hpp"
#include "NightConditionsCalculator.hpp"
#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "engine/SimpleEphemerisGuidanceStrategy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/CalendarTime.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::core::UtcTimePoint utcFromUnixSeconds(const qint64 seconds)
{
    return skygate::core::UtcTimePoint(std::chrono::seconds(seconds));
}

[[nodiscard]] skygate::core::ObservationContext makeZurichContext()
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = 47.3769, .longitudeDeg = 8.5417, .elevationMeters = 408.0};
    context.utcTime = utcFromUnixSeconds(1'711'024'800);  // 2024-03-21 12:00:00 UTC
    return context;
}

[[nodiscard]] skygate::core::ObservationContext makePolarSummerContext()
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = 80.0, .longitudeDeg = 0.0, .elevationMeters = 0.0};
    context.utcTime = utcFromUnixSeconds(1'719'576'000);  // 2024-06-30 12:00:00 UTC
    return context;
}

[[nodiscard]] std::optional<std::uint32_t>
bodyIndexById(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const std::string_view bodyId)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == bodyId) {
            return static_cast<std::uint32_t>(index);
        }
    }
    return std::nullopt;
}

struct TestRig final {
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> engine;
    std::uint32_t sunIndex = 0;
    std::uint32_t moonIndex = 0;
};

[[nodiscard]] TestRig makeTestRig()
{
    TestRig rig;
    rig.catalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    Q_ASSERT(rig.catalog != nullptr);
    rig.engine = std::move(skygate::ephemeris::EphemerisEngineFactory::create(*rig.catalog).engine);
    Q_ASSERT(rig.engine != nullptr);
    const auto sunIndex = bodyIndexById(rig.catalog->bodies(), "sun");
    const auto moonIndex = bodyIndexById(rig.catalog->bodies(), "moon");
    Q_ASSERT(sunIndex.has_value());
    Q_ASSERT(moonIndex.has_value());
    rig.sunIndex = *sunIndex;
    rig.moonIndex = *moonIndex;
    return rig;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody
toOwnGalaxyBody(const skygate::ephemeris::BaseCelestialBody& body)
{
    skygate::ephemeris::OwnGalaxyCelestialBody ownGalaxyBody;
    ownGalaxyBody.id = body.id;
    ownGalaxyBody.displayName = body.displayName;
    ownGalaxyBody.kind = body.kind;
    ownGalaxyBody.visualMagnitude = body.visualMagnitude;
    ownGalaxyBody.fixedEquatorial = body.fixedEquatorialValue();
    ownGalaxyBody.starAstrometry = body.starAstrometryValue();
    return ownGalaxyBody;
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog>
makeCatalog(std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies)
{
    return std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(std::move(bodies));
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog> makeSunMoonBodies()
{
    auto catalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    Q_ASSERT(catalog != nullptr);
    const auto sunIndex = bodyIndexById(catalog->bodies(), "sun");
    const auto moonIndex = bodyIndexById(catalog->bodies(), "moon");
    Q_ASSERT(sunIndex.has_value());
    Q_ASSERT(moonIndex.has_value());
    return makeCatalog(
        {toOwnGalaxyBody(*catalog->bodies()[*sunIndex]), toOwnGalaxyBody(*catalog->bodies()[*moonIndex])}
    );
}

[[nodiscard]] bool isAvailable(const skygate::ephemeris::ObservationEvent& event)
{
    return event.status == skygate::ephemeris::ObservationEventStatus::Type::Available && event.utcTime.has_value();
}

[[nodiscard]] skygate::ephemeris::tests::RequestCountingEphemerisEngine
makeGuidedNightEngine(std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog> bodies)
{
    auto engine = std::move(skygate::ephemeris::EphemerisEngineFactory::create(*bodies).engine);
    Q_ASSERT(engine != nullptr);
    return skygate::ephemeris::tests::RequestCountingEphemerisEngine(
        std::move(engine), skygate::ephemeris::tests::highPrecisionLightTimeOptions(), std::move(bodies)
    );
}

using skygate::ephemeris::tests::MarkerGuidanceStrategy;
using skygate::ephemeris::tests::TrustOptOutPrimaryEngine;

}  // namespace

class NightConditionsCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void twilightEventsAreOrderedForOrdinaryLocation();
    void invalidObserverReturnsUnavailableConditions();
    void polarTwilightReportsStableUnavailableStatuses();
    void moonRiseSetAndIlluminationArePopulated();
    void lunarPhaseBucketsAreDeterministic();
    void requestEpochControlsLunarPhaseWhenTimesDiffer();
    void highPrecisionNightConditionsUseGuidedEventSearch();
    void guidanceStrategyInjectionIsHonored();
    void nullGuidanceStrategyFallsBackToDirectSampling();
    void untrustedApproximateVerifiesAgainstPrimary();
    void trustedApproximateSkipPrimaryVerification();
};

void NightConditionsCalculatorTests::twilightEventsAreOrderedForOrdinaryLocation()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    const auto request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), rig.engine->options());

    const auto conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);

    QVERIFY(conditions.valid);
    QVERIFY(isAvailable(conditions.sunset));
    QVERIFY(isAvailable(conditions.civilDusk));
    QVERIFY(isAvailable(conditions.nauticalDusk));
    QVERIFY(isAvailable(conditions.astronomicalDusk));
    QVERIFY(isAvailable(conditions.astronomicalDawn));
    QVERIFY(isAvailable(conditions.sunrise));
    QVERIFY(*conditions.sunset.utcTime < *conditions.civilDusk.utcTime);
    QVERIFY(*conditions.civilDusk.utcTime < *conditions.nauticalDusk.utcTime);
    QVERIFY(*conditions.nauticalDusk.utcTime < *conditions.astronomicalDusk.utcTime);
    QVERIFY(*conditions.astronomicalDusk.utcTime < *conditions.astronomicalDawn.utcTime);
    QVERIFY(*conditions.astronomicalDawn.utcTime < *conditions.sunrise.utcTime);
}

void NightConditionsCalculatorTests::invalidObserverReturnsUnavailableConditions()
{
    auto rig = makeTestRig();
    auto context = makeZurichContext();
    context.observer.latitudeDeg = 120.0;
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    const auto request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, rig.engine->options());

    const auto conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);

    QVERIFY(!conditions.valid);
    QCOMPARE(conditions.sunrise.status, skygate::ephemeris::ObservationEventStatus::Type::Unresolved);
    QCOMPARE(conditions.moonrise.status, skygate::ephemeris::ObservationEventStatus::Type::Unresolved);
    QCOMPARE(conditions.moonIlluminationPercent, 0.0);
    QVERIFY(conditions.moonPhaseName.empty());
}

void NightConditionsCalculatorTests::polarTwilightReportsStableUnavailableStatuses()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    const auto request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(
        makePolarSummerContext(), rig.engine->options()
    );

    const auto conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);

    QVERIFY(conditions.valid);
    QVERIFY(conditions.sunrise.status != skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
    QVERIFY(conditions.sunset.status != skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
    QVERIFY(conditions.astronomicalDusk.status != skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
    QVERIFY(conditions.astronomicalDawn.status != skygate::ephemeris::ObservationEventStatus::Type::InvalidInput);
}

void NightConditionsCalculatorTests::moonRiseSetAndIlluminationArePopulated()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    const auto request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), rig.engine->options());

    const auto conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);

    QVERIFY(conditions.valid);
    QVERIFY(isAvailable(conditions.moonrise));
    QVERIFY(isAvailable(conditions.moonset));
    QVERIFY(conditions.moonIlluminationPercent >= 0.0);
    QVERIFY(conditions.moonIlluminationPercent <= 100.0);
    QVERIFY(!conditions.moonPhaseName.empty());
}

void NightConditionsCalculatorTests::lunarPhaseBucketsAreDeterministic()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    auto context = makeZurichContext();
    const auto engineOptions = rig.engine->options();

    context.utcTime = utcFromUnixSeconds(947'182'440);  // 2000-01-06 18:14:00 UTC
    auto request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engineOptions);
    auto conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("New Moon"));
    QVERIFY(conditions.moonIlluminationPercent < 1.0);

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engineOptions);
    conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("First quarter"));

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engineOptions);
    conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("Full Moon"));
    QVERIFY(conditions.moonIlluminationPercent > 99.0);

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engineOptions);
    conditions = calculator.compute(*rig.engine, request, rig.sunIndex, nullptr, rig.moonIndex, nullptr);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("Last quarter"));
}

void NightConditionsCalculatorTests::requestEpochControlsLunarPhaseWhenTimesDiffer()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine engine(
        std::vector<skygate::ephemeris::tests::FixedAltitudeBody>{
            {
                .body = skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 0.0, 12.0),
                .altitudeDeg = 12.0,
            },
            {
                .body = skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 1.0, 24.0),
                .altitudeDeg = 24.0,
            },
        }
    );
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeZurichContext();
    request.context.utcTime = utcFromUnixSeconds(1'711'024'800);  // 2024-03-21 12:00:00 UTC
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2000,
            .month = 1,
            .day = 6,
            .hour = 18,
            .minute = 14,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );

    const auto conditions = calculator.compute(engine, request, 0U, nullptr, 1U, nullptr);

    QVERIFY(conditions.valid);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("New Moon"));
    QVERIFY(conditions.moonIlluminationPercent < 1.0);
}

void NightConditionsCalculatorTests::highPrecisionNightConditionsUseGuidedEventSearch()
{
    const auto bodies = makeCatalog({
        skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
        skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
    });
    const auto engine = makeGuidedNightEngine(bodies);
    const auto strategy = std::make_shared<skygate::ephemeris::SimpleEphemerisGuidanceStrategy>();
    const skygate::ephemeris::NightConditionsCalculator calculator(strategy);
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(engine, request, 0U, &bodies->bodyAt(0), 1U, &bodies->bodyAt(1));

    QVERIFY(conditions.valid);
    QCOMPARE(engine.contextSampleCount(), 0);
    QCOMPARE(engine.requestSampleCount(), 2);
}

void NightConditionsCalculatorTests::guidanceStrategyInjectionIsHonored()
{
    const auto bodies = makeCatalog({
        skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
        skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
    });
    const auto engine = makeGuidedNightEngine(bodies);
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::NightConditionsCalculator calculator(strategy);
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(engine, request, 0U, &bodies->bodyAt(0), 1U, &bodies->bodyAt(1));

    QVERIFY(conditions.valid);
    QCOMPARE(strategy->createCount, 5);
    QVERIFY(*strategy->m_sampleCount > 0);
    QCOMPARE(engine.requestSampleCount(), 2);
    QCOMPARE(engine.contextSampleCount(), 0);
}

void NightConditionsCalculatorTests::nullGuidanceStrategyFallsBackToDirectSampling()
{
    const auto bodies = makeCatalog({
        skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
        skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
    });
    const auto engine = makeGuidedNightEngine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator(nullptr);
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(engine, request, 0U, &bodies->bodyAt(0), 1U, &bodies->bodyAt(1));

    QVERIFY(conditions.valid);
    QVERIFY(engine.requestSampleCount() > 2);
    QCOMPARE(engine.contextSampleCount(), 0);
}

void NightConditionsCalculatorTests::untrustedApproximateVerifiesAgainstPrimary()
{
    const auto bodies = makeCatalog({
        skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
        skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
    });
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::NightConditionsCalculator calculator(strategy);
    const auto request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(engine, request, 0U, &bodies->bodyAt(0), 1U, &bodies->bodyAt(1));

    QVERIFY(conditions.valid);
    QCOMPARE(conditions.sunrise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(conditions.sunrise.utcTime.has_value());
    QVERIFY(*primaryCall > 2);

    const auto sunriseRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, *conditions.sunrise.utcTime);
    const auto sunriseState = engine.computeBodyState(sunriseRequest, std::size_t{0U});
    QVERIFY(sunriseState.has_value());
    QVERIFY(std::abs(sunriseState->horizontal.altitudeDeg - (-0.833)) < 0.5);
}

void NightConditionsCalculatorTests::trustedApproximateSkipPrimaryVerification()
{
    const auto bodies = makeCatalog({
        skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
        skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
    });
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, true);
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::NightConditionsCalculator calculator(strategy);
    const auto request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(engine, request, 0U, &bodies->bodyAt(0), 1U, &bodies->bodyAt(1));

    QVERIFY(conditions.valid);
    QCOMPARE(conditions.sunrise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(*primaryCall, 2);
}

QTEST_APPLESS_MAIN(NightConditionsCalculatorTests)

#include "NightConditionsCalculatorTests.moc"
