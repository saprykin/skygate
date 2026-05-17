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

[[nodiscard]] bool isAvailable(const skygate::ephemeris::ObservationEvent& event)
{
    return event.status == skygate::ephemeris::ObservationEventStatus::Available && event.utcTime.has_value();
}

class FixedNightEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.states.push_back(*computeBodyState(context, 0U));
        snapshot.states.push_back(*computeBodyState(context, 1U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view bodyId) const override
    {
        if (bodyId == "sun") {
            return stateFor(0U, 12.0);
        }
        if (bodyId == "moon") {
            return stateFor(1U, 24.0);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t bodyIndex) const override
    {
        if (bodyIndex > 1U) {
            return std::nullopt;
        }
        return stateFor(bodyIndex, bodyIndex == 0U ? 12.0 : 24.0);
    }

private:
    [[nodiscard]] skygate::ephemeris::CelestialBodyState
    stateFor(const std::uint32_t bodyIndex, const double altitudeDeg) const noexcept
    {
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = bodyIndex,
            .equatorial = {.rightAscensionHours = static_cast<double>(bodyIndex), .declinationDeg = altitudeDeg},
            .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = 180.0}
        };
    }
};

[[nodiscard]] skygate::ephemeris::CelestialBody
makeFixedBody(std::string id, const double rightAscensionHours, const double declinationDeg)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.type = skygate::ephemeris::CelestialBodyType::Star;
    body.ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    return body;
}

class GuidedNightEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit GuidedNightEngine(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
        auto catalog = skygate::ephemeris::createStarCatalogFromBodies(*m_bodies);
        Q_ASSERT(catalog != nullptr);
        m_engine = skygate::ephemeris::createEphemerisEngine(*catalog);
        Q_ASSERT(m_engine != nullptr);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineOptions options;
        options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
        options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
        return options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot = m_engine->compute(request);
        snapshot.catalogBodies = m_bodies;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        ++requestSampleCount;
        return m_engine->computeBodyState(request, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++requestSampleCount;
        return m_engine->computeBodyState(request, bodyIndex);
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot = m_engine->compute(context);
        snapshot.catalogBodies = m_bodies;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        ++contextSampleCount;
        return m_engine->computeBodyState(context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        ++contextSampleCount;
        return m_engine->computeBodyState(context, bodyIndex);
    }

    std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> m_bodies;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_engine;
    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
};

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
    const FixedNightEngine engine;
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
    const std::vector<skygate::ephemeris::CelestialBody> bodies{
        makeFixedBody("sun", 8.0, 20.0),
        makeFixedBody("moon", 14.0, -8.0),
    };
    const GuidedNightEngine approximateEngine(bodies);
    const GuidedNightEngine verifiedEngine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator;
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::fromContext(makeZurichContext(), approximateEngine.options());

    const auto approximateConditions = calculator.compute(approximateEngine, request, 0U, bodies[0], 1U, bodies[1]);
    const auto verifiedConditions = calculator.compute(
        verifiedEngine,
        request,
        0U,
        bodies[0],
        1U,
        bodies[1],
        skygate::ephemeris::NightConditionsEventSearchMode::Verified
    );

    QVERIFY(approximateConditions.valid);
    QVERIFY(verifiedConditions.valid);
    QCOMPARE(approximateEngine.contextSampleCount, 0);
    QCOMPARE(verifiedEngine.contextSampleCount, 0);
    QCOMPARE(approximateEngine.requestSampleCount, 2);
    QVERIFY(approximateEngine.requestSampleCount < verifiedEngine.requestSampleCount);
}

void NightConditionsCalculatorTests::verifiedHighPrecisionNightConditionsUseSelectedEngineSamples()
{
    const std::vector<skygate::ephemeris::CelestialBody> bodies{
        makeFixedBody("sun", 8.0, 20.0),
        makeFixedBody("moon", 14.0, -8.0),
    };
    const GuidedNightEngine engine(bodies);
    const skygate::ephemeris::NightConditionsCalculator calculator;
    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::fromContext(makeZurichContext(), engine.options());

    const auto conditions = calculator.compute(
        engine, request, 0U, bodies[0], 1U, bodies[1], skygate::ephemeris::NightConditionsEventSearchMode::Verified
    );

    QVERIFY(conditions.valid);
    QVERIFY(engine.requestSampleCount > 2);
    QCOMPARE(engine.contextSampleCount, 0);
}

QTEST_APPLESS_MAIN(NightConditionsCalculatorTests)

#include "NightConditionsCalculatorTests.moc"
