#include "EphemerisEngineTestDoubles.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/EphemerisRequestFactory.hpp"
#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include <QtTest/QtTest>

#include <chrono>
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

[[nodiscard]] skygate::core::SkyContext makeZurichContext()
{
    skygate::core::SkyContext context;
    context.observer = {.latitudeDeg = 47.3769, .longitudeDeg = 8.5417, .elevationMeters = 408.0};
    context.utcTime = utcFromUnixSeconds(1'711'024'800);  // 2024-03-21 12:00:00 UTC
    return context;
}

[[nodiscard]] skygate::core::SkyContext makePolarSummerContext()
{
    skygate::core::SkyContext context;
    context.observer = {.latitudeDeg = 80.0, .longitudeDeg = 0.0, .elevationMeters = 0.0};
    context.utcTime = utcFromUnixSeconds(1'719'576'000);  // 2024-06-30 12:00:00 UTC
    return context;
}

[[nodiscard]] std::optional<std::uint32_t>
bodyIndexById(const std::span<const skygate::ephemeris::CelestialBody> bodies, const std::string_view bodyId)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index].id == bodyId) {
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
    rig.catalog = skygate::ephemeris::createBundledStarCatalog();
    Q_ASSERT(rig.catalog != nullptr);
    rig.engine = skygate::ephemeris::createEphemerisEngine(*rig.catalog);
    Q_ASSERT(rig.engine != nullptr);
    const auto sunIndex = bodyIndexById(rig.catalog->bodies(), "sun");
    const auto moonIndex = bodyIndexById(rig.catalog->bodies(), "moon");
    Q_ASSERT(sunIndex.has_value());
    Q_ASSERT(moonIndex.has_value());
    rig.sunIndex = *sunIndex;
    rig.moonIndex = *moonIndex;
    return rig;
}

[[nodiscard]] std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> makeSunMoonBodies()
{
    auto catalog = skygate::ephemeris::createBundledStarCatalog();
    Q_ASSERT(catalog != nullptr);
    const auto sunIndex = bodyIndexById(catalog->bodies(), "sun");
    const auto moonIndex = bodyIndexById(catalog->bodies(), "moon");
    Q_ASSERT(sunIndex.has_value());
    Q_ASSERT(moonIndex.has_value());
    return std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
        std::vector<skygate::ephemeris::CelestialBody>{catalog->bodies()[*sunIndex], catalog->bodies()[*moonIndex]}
    );
}

[[nodiscard]] bool isAvailable(const skygate::ephemeris::ObservationEvent& event)
{
    return event.status == skygate::ephemeris::ObservationEventStatus::Available && event.utcTime.has_value();
}

[[nodiscard]] skygate::ephemeris::tests::RequestCountingEphemerisEngine
makeGuidedNightEngine(std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> bodies)
{
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(*bodies);
    Q_ASSERT(catalog != nullptr);
    auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    Q_ASSERT(engine != nullptr);
    return skygate::ephemeris::tests::RequestCountingEphemerisEngine(
        std::move(engine), skygate::ephemeris::tests::highPrecisionLightTimeOptions(), std::move(bodies)
    );
}

}  // namespace

class NightConditionsCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void twilightEventsAreOrderedForOrdinaryLocation();
    void invalidObserverReturnsUnavailableConditions();
    void polarTwilightReportsStableUnavailableStatuses();
    void moonRiseSetAndIlluminationArePopulated();
    void lunarPhaseBucketsAreDeterministic();
    void requestEpochControlsLunarPhaseWhenContextTimeDiffers();
    void highPrecisionNightConditionsUseGuidedEventSearch();
    void approximateHighPrecisionSunMoonEventsUseSimpleEstimates();
    void verifiedHighPrecisionNightConditionsUseSelectedEngineSamples();
};

void NightConditionsCalculatorTests::twilightEventsAreOrderedForOrdinaryLocation()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator;

    const auto conditions = calculator.compute(*rig.engine, makeZurichContext(), rig.sunIndex, rig.moonIndex);

    QVERIFY(conditions.valid);
    QVERIFY(conditions.sunAltitudeDeg.has_value());
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
    const skygate::ephemeris::NightConditionsCalculator calculator;

    const auto conditions = calculator.compute(*rig.engine, context, rig.sunIndex, rig.moonIndex);

    QVERIFY(!conditions.valid);
    QVERIFY(!conditions.sunAltitudeDeg.has_value());
    QCOMPARE(conditions.sunrise.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(conditions.moonrise.status, skygate::ephemeris::ObservationEventStatus::Unresolved);
    QCOMPARE(conditions.moonIlluminationPercent, 0.0);
    QVERIFY(conditions.moonPhaseName.empty());
}

void NightConditionsCalculatorTests::polarTwilightReportsStableUnavailableStatuses()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator;

    const auto conditions = calculator.compute(*rig.engine, makePolarSummerContext(), rig.sunIndex, rig.moonIndex);

    QVERIFY(conditions.valid);
    QVERIFY(conditions.sunAltitudeDeg.has_value());
    QVERIFY(conditions.sunrise.status != skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QVERIFY(conditions.sunset.status != skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QVERIFY(conditions.astronomicalDusk.status != skygate::ephemeris::ObservationEventStatus::InvalidInput);
    QVERIFY(conditions.astronomicalDawn.status != skygate::ephemeris::ObservationEventStatus::InvalidInput);
}

void NightConditionsCalculatorTests::moonRiseSetAndIlluminationArePopulated()
{
    auto rig = makeTestRig();
    const skygate::ephemeris::NightConditionsCalculator calculator;

    const auto conditions = calculator.compute(*rig.engine, makeZurichContext(), rig.sunIndex, rig.moonIndex);

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
    const skygate::ephemeris::NightConditionsCalculator calculator;
    auto context = makeZurichContext();

    context.utcTime = utcFromUnixSeconds(947'182'440);  // 2000-01-06 18:14:00 UTC
    auto conditions = calculator.compute(*rig.engine, context, rig.sunIndex, rig.moonIndex);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("New Moon"));
    QVERIFY(conditions.moonIlluminationPercent < 1.0);

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    conditions = calculator.compute(*rig.engine, context, rig.sunIndex, rig.moonIndex);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("First quarter"));

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    conditions = calculator.compute(*rig.engine, context, rig.sunIndex, rig.moonIndex);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("Full Moon"));
    QVERIFY(conditions.moonIlluminationPercent > 99.0);

    context.utcTime += std::chrono::seconds(7 * 86400 + 9 * 3600);
    conditions = calculator.compute(*rig.engine, context, rig.sunIndex, rig.moonIndex);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("Last quarter"));
}

void NightConditionsCalculatorTests::requestEpochControlsLunarPhaseWhenContextTimeDiffers()
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
    const skygate::ephemeris::NightConditionsCalculator calculator;
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeZurichContext();
    request.context.utcTime = utcFromUnixSeconds(1'711'024'800);  // 2024-03-21 12:00:00 UTC
    request.epoch = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 2000,
            .month = 1,
            .day = 6,
            .hour = 18,
            .minute = 14,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );

    const auto conditions = calculator.compute(engine, request, 0U, 1U);

    QVERIFY(conditions.valid);
    QCOMPARE(QString::fromStdString(conditions.moonPhaseName), QString("New Moon"));
    QVERIFY(conditions.moonIlluminationPercent < 1.0);
}

void NightConditionsCalculatorTests::highPrecisionNightConditionsUseGuidedEventSearch()
{
    const auto bodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
        std::vector<skygate::ephemeris::CelestialBody>{
            skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
            skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
        }
    );
    const auto approximateEngine = makeGuidedNightEngine(bodies);
    const auto verifiedEngine = makeGuidedNightEngine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator;
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::fromContext(makeZurichContext(), approximateEngine.options());

    const auto approximateConditions =
        calculator.compute(approximateEngine, request, 0U, (*bodies)[0], 1U, (*bodies)[1]);
    const auto verifiedConditions = calculator.compute(
        verifiedEngine,
        request,
        0U,
        (*bodies)[0],
        1U,
        (*bodies)[1],
        skygate::ephemeris::NightConditionsEventSearchMode::Verified
    );

    QVERIFY(approximateConditions.valid);
    QVERIFY(verifiedConditions.valid);
    QCOMPARE(approximateEngine.contextSampleCount(), 0);
    QCOMPARE(verifiedEngine.contextSampleCount(), 0);
    QCOMPARE(approximateEngine.requestSampleCount(), 2);
    QVERIFY(approximateEngine.requestSampleCount() < verifiedEngine.requestSampleCount());
}

void NightConditionsCalculatorTests::approximateHighPrecisionSunMoonEventsUseSimpleEstimates()
{
    const auto bodies = makeSunMoonBodies();
    const auto approximateEngine = makeGuidedNightEngine(bodies);
    const auto verifiedEngine = makeGuidedNightEngine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator;
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::fromContext(makeZurichContext(), approximateEngine.options());

    const auto approximateConditions =
        calculator.compute(approximateEngine, request, 0U, (*bodies)[0], 1U, (*bodies)[1]);
    const auto verifiedConditions = calculator.compute(
        verifiedEngine,
        request,
        0U,
        (*bodies)[0],
        1U,
        (*bodies)[1],
        skygate::ephemeris::NightConditionsEventSearchMode::Verified
    );

    QVERIFY(approximateConditions.valid);
    QVERIFY(verifiedConditions.valid);
    QCOMPARE(approximateEngine.contextSampleCount(), 0);
    QCOMPARE(verifiedEngine.contextSampleCount(), 0);
    QCOMPARE(approximateEngine.requestSampleCount(), 2);
    QVERIFY(verifiedEngine.requestSampleCount() > 300);
}

void NightConditionsCalculatorTests::verifiedHighPrecisionNightConditionsUseSelectedEngineSamples()
{
    const auto bodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
        std::vector<skygate::ephemeris::CelestialBody>{
            skygate::ephemeris::tests::makeFixedAltitudeBody("sun", 8.0, 20.0),
            skygate::ephemeris::tests::makeFixedAltitudeBody("moon", 14.0, -8.0),
        }
    );
    const auto engine = makeGuidedNightEngine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator;
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::fromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(
        engine,
        request,
        0U,
        (*bodies)[0],
        1U,
        (*bodies)[1],
        skygate::ephemeris::NightConditionsEventSearchMode::Verified
    );

    QVERIFY(conditions.valid);
    QVERIFY(engine.requestSampleCount() > 2);
    QCOMPARE(engine.contextSampleCount(), 0);
}

QTEST_APPLESS_MAIN(NightConditionsCalculatorTests)

#include "NightConditionsCalculatorTests.moc"
