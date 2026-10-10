#include "EphemerisEngineTestDoubles.hpp"
#include "MarkerGuidanceStrategy.hpp"
#include "ObservationEventCalculator.hpp"
#include "UtcTimeCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "engine/SimpleEphemerisGuidanceStrategy.hpp"
#include "factory/EphemerisEngineFactory.hpp"
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

constexpr double kPi = 3.14159265358979323846;

skygate::core::ObservationContext makeContext(const double latitudeDeg = 47.0, const double longitudeDeg = 8.0)
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = latitudeDeg, .longitudeDeg = longitudeDeg, .elevationMeters = 400.0};
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return context;
}

skygate::ephemeris::OwnGalaxyCelestialBody makeFixedBody(const skygate::core::EquatorialCoordinate& equatorial)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "target";
    body.displayName = "Target";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.visualMagnitude = 1.0;
    body.fixedEquatorial = equatorial;
    return body;
}

std::unique_ptr<skygate::ephemeris::IEphemerisEngine>
makeEngineForBody(const skygate::ephemeris::OwnGalaxyCelestialBody& body)
{
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{body}
    );
    Q_ASSERT(catalog != nullptr);
    return std::move(skygate::ephemeris::EphemerisEngineFactory::create(*catalog).engine);
}

class LinearAltitudeGuidanceEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    LinearAltitudeGuidanceEngine(const skygate::core::UtcTimePoint crossingTime, const double slopeDegPerSecond)
        : m_crossingTime(crossingTime), m_slopeDegPerSecond(slopeDegPerSecond)
    {
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Linear altitude guidance test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
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
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::string_view) const override
    {
        return computeBodyState(context, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(context.utcTime);
        const double crossingSeconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(m_crossingTime);
        const double altitudeDeg = (seconds - crossingSeconds) * m_slopeDegPerSecond;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = 180.0}
        };
    }

private:
    skygate::core::UtcTimePoint m_crossingTime;
    double m_slopeDegPerSecond;
};

class LinearAltitudeGuidanceStrategy final : public skygate::ephemeris::IEphemerisGuidanceStrategy {
public:
    LinearAltitudeGuidanceStrategy(const skygate::core::UtcTimePoint crossingTime, const double slopeDegPerSecond)
        : m_crossingTime(crossingTime), m_slopeDegPerSecond(slopeDegPerSecond)
    {
    }

    [[nodiscard]] std::unique_ptr<skygate::ephemeris::IEphemerisEngine>
    createGuidanceEngine(const skygate::ephemeris::CelestialBodyCatalog&) const override
    {
        return std::make_unique<LinearAltitudeGuidanceEngine>(m_crossingTime, m_slopeDegPerSecond);
    }

private:
    skygate::core::UtcTimePoint m_crossingTime;
    double m_slopeDegPerSecond;
};

using skygate::ephemeris::tests::MarkerGuidanceStrategy;
using SearchMode = skygate::ephemeris::ObservationEventCalculator::SearchMode;

}  // namespace

class ObservationEventGuidanceTests final : public QObject {
    Q_OBJECT

private slots:
    void guidanceStrategyInjectionIsHonored();
    void highPrecisionFixedBodyUsesGuidedCoarseSearch();
    void linearGuidanceInterpolatesCrossingTimeInCorrectUnits();
    void nullGuidanceStrategyFallsBackToDirectSampling();
};

void ObservationEventGuidanceTests::guidanceStrategyInjectionIsHonored()
{
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    const skygate::ephemeris::tests::RequestCountingEphemerisEngine engine(
        makeEngineForBody(body), skygate::ephemeris::tests::highPrecisionLightTimeOptions()
    );
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2024,
            .month = 6,
            .day = 2,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    request.options = engine.options();

    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(strategy->createCount, 1);
    QVERIFY(*strategy->m_sampleCount > 0);
    QCOMPARE(engine.requestSampleCount(), 0);
    QCOMPARE(engine.contextSampleCount(), 0);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
}

void ObservationEventGuidanceTests::highPrecisionFixedBodyUsesGuidedCoarseSearch()
{
    const auto strategy = std::make_shared<skygate::ephemeris::SimpleEphemerisGuidanceStrategy>();
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    const skygate::ephemeris::tests::RequestCountingEphemerisEngine engine(
        makeEngineForBody(body), skygate::ephemeris::tests::highPrecisionLightTimeOptions()
    );
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2024,
            .month = 6,
            .day = 2,
            .timeScale = skygate::core::TimeScale::Utc,
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

void ObservationEventGuidanceTests::linearGuidanceInterpolatesCrossingTimeInCorrectUnits()
{
    constexpr double kSlopeDegPerSecond = 60.0 / 600.0;
    const auto context = makeContext();
    const skygate::core::UtcTimePoint expectedRiseUtc = context.utcTime + std::chrono::seconds(300);

    const auto strategy = std::make_shared<LinearAltitudeGuidanceStrategy>(expectedRiseUtc, kSlopeDegPerSecond);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    const skygate::ephemeris::tests::RequestCountingEphemerisEngine engine(
        makeEngineForBody(body), skygate::ephemeris::tests::highPrecisionLightTimeOptions()
    );
    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2024,
            .month = 6,
            .day = 2,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    request.options = engine.options();

    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QCOMPARE((*summary.nextRise.utcTime).time_since_epoch().count(), expectedRiseUtc.time_since_epoch().count());
    QVERIFY(*summary.nextRise.utcTime >= context.utcTime);
    QVERIFY(*summary.nextRise.utcTime <= context.utcTime + std::chrono::seconds(72 * 60 * 60));

    const LinearAltitudeGuidanceEngine guidance(expectedRiseUtc, kSlopeDegPerSecond);
    skygate::ephemeris::EphemerisRequest verificationRequest = request;
    verificationRequest.context.utcTime = *summary.nextRise.utcTime;
    const auto verificationState = guidance.computeBodyState(verificationRequest, std::size_t{0U});
    QVERIFY(verificationState.has_value());
    QVERIFY(std::abs(verificationState->horizontal.altitudeDeg - 0.0) < 1e-9);
}

void ObservationEventGuidanceTests::nullGuidanceStrategyFallsBackToDirectSampling()
{
    const skygate::ephemeris::ObservationEventCalculator calculator(nullptr);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    const skygate::ephemeris::tests::RequestCountingEphemerisEngine engine(
        makeEngineForBody(body), skygate::ephemeris::tests::highPrecisionLightTimeOptions()
    );
    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.epoch = *skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2024,
            .month = 6,
            .day = 2,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    request.options = engine.options();

    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(engine.contextSampleCount(), 0);
    QVERIFY(engine.requestSampleCount() > 0);
    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Available);
    QCOMPARE(summary.culmination.status, skygate::ephemeris::ObservationEventStatus::Available);
}

QTEST_APPLESS_MAIN(ObservationEventGuidanceTests)

#include "ObservationEventGuidanceTests.moc"
